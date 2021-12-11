#include "AccelerationStructure.h"
#include "Device.h"
#include "VulkanDebug.h"
#include "VulkanFunctions.h"
#include <iostream>

namespace genesis
{
   AccelerationStructure::AccelerationStructure(Device* device, AccelerationStructureType type, uint64_t sizeInBytes)
      : _device(device)
      , _type(type)
   {
      VkBufferCreateInfo bufferCreateInfo{};
      bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferCreateInfo.size = sizeInBytes;
      bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
      VK_CHECK_RESULT(vkCreateBuffer(_device->vulkanDevice(), &bufferCreateInfo, nullptr, &_buffer));

      VkMemoryRequirements memoryRequirements{};
      vkGetBufferMemoryRequirements(_device->vulkanDevice(), _buffer, &memoryRequirements);
      VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
      memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
      memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

      VkMemoryAllocateInfo memoryAllocateInfo{};
      memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
      memoryAllocateInfo.allocationSize = memoryRequirements.size;
      memoryAllocateInfo.memoryTypeIndex = _device->getMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memoryAllocateInfo, nullptr, &_memory));

      VK_CHECK_RESULT(vkBindBufferMemory(_device->vulkanDevice(), _buffer, _memory, 0));

      // Acceleration structure
      VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
      accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
      accelerationStructureCreateInfo.buffer = _buffer;
      accelerationStructureCreateInfo.size = sizeInBytes;

      VkAccelerationStructureTypeKHR vulkanAsType;
      switch (_type)
      {
      case genesis::BLAS:
         vulkanAsType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
         break;
      case genesis::TLAS:
         vulkanAsType = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
         break;
      default:
         vulkanAsType = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
         std::cout << __FILE__ << __LINE__ << "unknown type" << std::endl;
         break;
      }
      accelerationStructureCreateInfo.type = vulkanAsType;
      vkCreateAccelerationStructureKHR(_device->vulkanDevice(), &accelerationStructureCreateInfo, nullptr, &_handle);

      // AS device address
      VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
      accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
      accelerationStructureDeviceAddressInfo.accelerationStructure = _handle;
      _deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(_device->vulkanDevice(), &accelerationStructureDeviceAddressInfo);
   }

   AccelerationStructure::~AccelerationStructure()
   {
      vkFreeMemory(_device->vulkanDevice(), _memory, nullptr);
      vkDestroyBuffer(_device->vulkanDevice(), _buffer, nullptr);
      vkDestroyAccelerationStructureKHR(_device->vulkanDevice(), _handle, nullptr);
   }

   const VkAccelerationStructureKHR& AccelerationStructure::handle(void) const
   {
      return _handle;
   }

   uint64_t AccelerationStructure::deviceAddress(void) const
   {
      return _deviceAddress;
   }
}