#include "Image.h"
#include "Device.h"
#include "Buffer.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"
#include "ImageTransitions.h"

#include "GenAssert.h"

#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>

namespace genesis
{
   Image::Image(Device* _device)
      : _device(_device)
   {

   }

   Image::~Image()
   {
      VkDevice vulkanDevice = _device->vulkanDevice();

      vkDestroyImage(vulkanDevice, _image, nullptr);
      vkFreeMemory(vulkanDevice, _deviceMemory, nullptr);
   }

   void Image::allocateImageAndMemory(VkImageUsageFlags usage)
   {
      VkImageCreateInfo imageCreateInfo = VulkanInitializers::imageCreateInfo();
      imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
      imageCreateInfo.format = _format;
      imageCreateInfo.extent = { static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), 1 };
      imageCreateInfo.mipLevels = _numMipMapLevels;
      imageCreateInfo.arrayLayers = 1;
      imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageCreateInfo.usage = usage;
      imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      VK_CHECK_RESULT(vkCreateImage(_device->vulkanDevice(), &imageCreateInfo, nullptr, &_image));

      VkMemoryRequirements memoryRequirements;
      vkGetImageMemoryRequirements(_device->vulkanDevice(), _image, &memoryRequirements);

      VkMemoryAllocateInfo memoryAllocateInfo = genesis::VulkanInitializers::memoryAllocateInfo();
      memoryAllocateInfo.allocationSize = memoryRequirements.size;
      memoryAllocateInfo.memoryTypeIndex = _device->getMemoryTypeIndex(memoryRequirements.memoryTypeBits
         , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memoryAllocateInfo, nullptr, &_deviceMemory));
      VK_CHECK_RESULT(vkBindImageMemory(_device->vulkanDevice(), _image, _deviceMemory, 0));
   }

   bool Image::copyFromRawDataIntoImage(void* pSrcData, VkDeviceSize pSrcDataSize, const std::vector<int>& mipMapDataOffsets)
   {
      VulkanBuffer* stagingBuffer = new VulkanBuffer(_device, BT_STAGING, (int)pSrcDataSize);
      uint8_t* pDstData = 0;
      VK_CHECK_RESULT(vkMapMemory(_device->vulkanDevice(), stagingBuffer->_deviceMemory, 0, pSrcDataSize, 0, (void**)&pDstData));
      memcpy(pDstData, pSrcData, pSrcDataSize);
      vkUnmapMemory(_device->vulkanDevice(), stagingBuffer->_deviceMemory);

      allocateImageAndMemory(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

      std::vector<VkBufferImageCopy> bufferCopyRegions;

      for (uint32_t i = 0; i < (uint32_t)_numMipMapLevels; ++i)
      {
         VkBufferImageCopy bufferImageCopy = {};

         bufferImageCopy.bufferOffset = mipMapDataOffsets[i];
         bufferImageCopy.bufferRowLength = 0;
         bufferImageCopy.bufferImageHeight = 0;

         bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         bufferImageCopy.imageSubresource.mipLevel = i;
         bufferImageCopy.imageSubresource.baseArrayLayer = 0;
         bufferImageCopy.imageSubresource.layerCount = 1;

         bufferImageCopy.imageOffset = { 0,0,0 };
         bufferImageCopy.imageExtent.width = _width >> i;
         bufferImageCopy.imageExtent.height = _height >> i;
         bufferImageCopy.imageExtent.depth = 1;

         bufferCopyRegions.push_back(bufferImageCopy);
      };

      VkCommandBuffer commandBuffer = _device->getCommandBuffer(true);

      VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT , 0, (uint32_t)_numMipMapLevels, 0, 1 };
      ImageTransitions transition;
      transition.setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

      vkCmdCopyBufferToImage(commandBuffer
         , stagingBuffer->vulkanBuffer()
         , _image
         , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
         , (uint32_t)bufferCopyRegions.size()
         , bufferCopyRegions.data()
      );

      transition.setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

      _device->flushCommandBuffer(commandBuffer);

      delete stagingBuffer;

      return true;
   }

   bool Image::copyFromFileIntoImage(const std::string& fileName)
   {
      std::ifstream ifs(fileName.c_str());
      if (ifs.fail())
      {
         return false;
      }

      ktxTexture* ktxTexture = nullptr;
      ktxResult result = ktxTexture_CreateFromNamedFile(fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

      GEN_ASSERT(result == KTX_SUCCESS);
      if (result != KTX_SUCCESS)
      {
         return false;
      }

      ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
      ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

      _numMipMapLevels = ktxTexture->numLevels;
      _width = ktxTexture->baseWidth;
      _height = ktxTexture->baseHeight;
      _format = VK_FORMAT_R8G8B8A8_UNORM;

      std::vector<int> dataOffsets;
      for (uint32_t i = 0; i < (uint32_t)_numMipMapLevels; ++i)
      {
         ktx_size_t offset = 0;
         KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
         if (ret != KTX_SUCCESS)
         {
            std::cout << __FUNCTION__ << "ret != KTX_SUCCESS" << std::endl;
         }
         dataOffsets.push_back((int)offset);
      }
      GEN_ASSERT(_numMipMapLevels == dataOffsets.size());
      if (_numMipMapLevels != dataOffsets.size())
      {
         std::cout << __FUNCTION__ << "numMipMapLevels != dataOffsets.size()" << std::endl;
      }

      copyFromRawDataIntoImage(ktxTextureData, ktxTextureSize, dataOffsets);

      ktxTexture_Destroy(ktxTexture);

      return true;
   }


   bool Image::loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, int width, int height, const std::vector<int>& mipMapDataOffsets)
   {
      GEN_ASSERT(buffer);
      if (buffer == nullptr)
      {
         std::cout << __FUNCTION__ << "(buffer==nullptr)" << std::endl;
         return false;
      }
      _numMipMapLevels = (int)mipMapDataOffsets.size();
      _width = width;
      _height = height;
      _format = format;

      bool ok = copyFromRawDataIntoImage(buffer, bufferSize, mipMapDataOffsets);
      if (!ok)
      {
         return false;
      }

      return ok;
   }

   bool Image::loadFromFile(const std::string& fileName)
   {
      bool ok = copyFromFileIntoImage(fileName);
      if (!ok)
      {
         return false;
      }
      return ok;
   }

   int Image::numMipMapLevels(void) const
   {
      return _numMipMapLevels;
   }

   int Image::width(void) const
   {
      return _width;
   }

   int Image::height(void) const
   {
      return _height;
   }

   VkFormat Image::vulkanFormat(void) const
   {
      return _format;
   }

   VkImage Image::vulkanImage(void) const
   {
      return _image;
   }

   const Device* Image::device(void) const
   {
      return _device;
   }
}

