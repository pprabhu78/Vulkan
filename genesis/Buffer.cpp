#include "Buffer.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanFunctions.h"
#include "VulkanDebug.h"
#include "VulkanInitializers.h"
#include "GenAssert.h"

namespace genesis
{

   VulkanBuffer::VulkanBuffer(Device* _device, BufferType bufferType, int sizeInBytes, const BufferProperties& bufferProperties)
      : _buffer(0)
      , _deviceMemory(0)
      , _device(_device)
   {
      VkBufferCreateInfo bufferCreateInfo = {};
      bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferCreateInfo.size = sizeInBytes;

      switch (bufferType)
      {
      case BT_STAGING:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
         break;
      case BT_VERTEX_BUFFER:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         break;
      case BT_INDEX_BUFFER:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         break;
      case BT_UBO:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         break;
      case BT_SBO:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         break;
      case BT_INDIRECT_BUFFER:
         bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         break;
      }

      if (bufferProperties._deviceAddressing)
      {
         bufferCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
      }

      if (bufferProperties._inputToAccelerationStructure)
      {
         bufferCreateInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
      }

      if ((bufferType == BT_VERTEX_BUFFER || bufferType == BT_INDEX_BUFFER) && bufferProperties._vertexOrIndexBoundAsSsbo)
      {
         bufferCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      }

      VK_CHECK_RESULT(vkCreateBuffer(_device->vulkanDevice(), &bufferCreateInfo, nullptr, &_buffer));

      VkMemoryRequirements memoryRequirements;
      vkGetBufferMemoryRequirements(_device->vulkanDevice(), _buffer, &memoryRequirements);

      VkMemoryAllocateInfo memoryAllocateInfo = VulkanInitializers::memoryAllocateInfo();
      memoryAllocateInfo.allocationSize = memoryRequirements.size;

      if (bufferProperties._deviceAddressing)
      {
         VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = VulkanInitializers::memoryAllocateFlagsInfo();
         memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
         memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
      }

      VkMemoryPropertyFlags memoryProperties;
      switch (bufferType)
      {
      case BT_STAGING:
         memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
         break;
      default:
         memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
         break;
      }

      memoryAllocateInfo.memoryTypeIndex = _device->physicalDevice()->getMemoryTypeIndex(memoryRequirements.memoryTypeBits
         , memoryProperties);

      VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memoryAllocateInfo, nullptr, &_deviceMemory));
      vkBindBufferMemory(_device->vulkanDevice(), _buffer, _deviceMemory, 0);
   }

   VulkanBuffer::~VulkanBuffer()
   {
      if (_buffer)
      {
         vkDestroyBuffer(_device->vulkanDevice(), _buffer, nullptr);
      }
      if (_deviceMemory)
      {
         vkFreeMemory(_device->vulkanDevice(), _deviceMemory, nullptr);
      }
   }

   VkBuffer VulkanBuffer::vulkanBuffer(void) const
   {
      return _buffer;
   }

   Buffer::Buffer(Device* device, BufferType bufferType, int sizeInBytes, bool staging, const BufferProperties& bufferProperties, const std::string& name)
      : _device(device)
      , _sizeInBytes(sizeInBytes)
      , _stagingBuffer(nullptr)
      , _buffer(nullptr)
   {
      if (staging)
      {
         _stagingBuffer = new VulkanBuffer(_device, BT_STAGING, _sizeInBytes);
      }

      _buffer = new VulkanBuffer(_device, bufferType, _sizeInBytes, bufferProperties);

      if (!name.empty())
      {
//         vks::debugmarker::setBufferName(_device->vulkanDevice(), _buffer->vulkanBuffer(), name.c_str());
      }

      _descriptor = VkDescriptorBufferInfo{ _buffer->vulkanBuffer(), 0, (VkDeviceSize)_sizeInBytes };
   }

   void* Buffer::stagingBuffer(void)
   {
      GEN_ASSERT(_stagingBuffer);

      void* data = nullptr;
      vkMapMemory(_device->vulkanDevice(), _stagingBuffer->_deviceMemory, 0, _sizeInBytes, 0, &data);

      return data;
   }

   VkBuffer Buffer::vulkanBuffer(void) const
   {
      return _buffer->_buffer;
   }

   bool Buffer::syncToGpu(bool destroyStaging)
   {
      GEN_ASSERT(_stagingBuffer);

      vkUnmapMemory(_device->vulkanDevice(), _stagingBuffer->_deviceMemory);

      if (_buffer)
      {
         VkCommandBuffer copyCmd = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
         VkBufferCopy bufferCopy = {};

         bufferCopy.size = _sizeInBytes;

         vkCmdCopyBuffer(copyCmd, _stagingBuffer->_buffer, _buffer->_buffer, 1, &bufferCopy);

         _device->flushCommandBuffer(copyCmd);
      }

      if (destroyStaging)
      {
         delete _stagingBuffer;
         _stagingBuffer = nullptr;
      }

      return true;
   }

   Buffer::~Buffer()
   {
      if (_stagingBuffer)
      {
         delete _stagingBuffer;
         _stagingBuffer = nullptr;
      }
      if (_buffer)
      {
         delete _buffer;
      }
   }

   int Buffer::sizeInBytes(void) const
   {
      return _sizeInBytes;
   }

   const VkDescriptorBufferInfo& Buffer::descriptor(void) const
   {
      return _descriptor;
   }

   const VkDescriptorBufferInfo* Buffer::descriptorPtr(void) const
   {
      return &_descriptor;
   }

   uint64_t Buffer::bufferAddress() const
   {
      VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
      bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
      bufferDeviceAI.buffer = _buffer->vulkanBuffer();
      return vkGetBufferDeviceAddressKHR(_device->vulkanDevice(), &bufferDeviceAI);
   }
}
