#include "StorageImage.h"
#include "VulkanTools.h"
#include "VulkanInitializers.h"
#include "Device.h"

namespace genesis
{
   StorageImage::StorageImage(Device* device, VkFormat format, int width, int height)
      : _device(device)
   {
      VkImageCreateInfo imageCreateInfo = vulkanInitializers::imageCreateInfo();
      imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
      imageCreateInfo.format = format;
      imageCreateInfo.extent.width = width;
      imageCreateInfo.extent.height = height;
      imageCreateInfo.extent.depth = 1;
      imageCreateInfo.mipLevels = 1;
      imageCreateInfo.arrayLayers = 1;
      imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
      imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      VK_CHECK_RESULT(vkCreateImage(_device->vulkanDevice(), &imageCreateInfo, nullptr, &_image));

      VkMemoryRequirements memReqs;
      vkGetImageMemoryRequirements(_device->vulkanDevice(), _image, &memReqs);
      VkMemoryAllocateInfo memoryAllocateInfo = genesis::vulkanInitializers::memoryAllocateInfo();
      memoryAllocateInfo.allocationSize = memReqs.size;
      memoryAllocateInfo.memoryTypeIndex = _device->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memoryAllocateInfo, nullptr, &_memory));
      VK_CHECK_RESULT(vkBindImageMemory(_device->vulkanDevice(), _image, _memory, 0));

      VkImageViewCreateInfo colorImageView = genesis::vulkanInitializers::imageViewCreateInfo();
      colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
      colorImageView.format = format;
      colorImageView.subresourceRange = {};
      colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      colorImageView.subresourceRange.baseMipLevel = 0;
      colorImageView.subresourceRange.levelCount = 1;
      colorImageView.subresourceRange.baseArrayLayer = 0;
      colorImageView.subresourceRange.layerCount = 1;
      colorImageView.image = _image;
      VK_CHECK_RESULT(vkCreateImageView(_device->vulkanDevice(), &colorImageView, nullptr, &_view));

      VkCommandBuffer cmdBuffer = _device->getCommandBuffer(true);
      vks::tools::setImageLayout(cmdBuffer, _image,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_GENERAL,
         { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
      _device->flushCommandBuffer(cmdBuffer);
   }

   StorageImage::~StorageImage()
   {
      vkDestroyImageView(_device->vulkanDevice(), _view, nullptr);
      vkDestroyImage(_device->vulkanDevice(), _image, nullptr);
      vkFreeMemory(_device->vulkanDevice(), _memory, nullptr);
   }
   const VkImageView& StorageImage::vulkanImageView(void) const
   {
      return _view;
   }

   const VkImage& StorageImage::vulkanImage(void) const
   {
      return _image;
   }
}