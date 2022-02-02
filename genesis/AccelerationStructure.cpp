#include "AccelerationStructure.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "VulkanFunctions.h"
#include "Buffer.h"
#include <iostream>

namespace genesis
{
   AccelerationStructure::AccelerationStructure(Device* device, VkAccelerationStructureTypeKHR type, uint64_t sizeInBytes)
      : _device(device)
      , _type(type)
   {
      const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
      const VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      _buffer = new VulkanBuffer(_device, bufferUsageFlags, memoryPropertyFlags, sizeInBytes);

      // Acceleration structure
      VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
      accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
      accelerationStructureCreateInfo.buffer = _buffer->vulkanBuffer();
      accelerationStructureCreateInfo.size = sizeInBytes;
      accelerationStructureCreateInfo.type = type;
      vkCreateAccelerationStructureKHR(_device->vulkanDevice(), &accelerationStructureCreateInfo, nullptr, &_handle);
   }

   AccelerationStructure::~AccelerationStructure()
   {
      vkDestroyAccelerationStructureKHR(_device->vulkanDevice(), _handle, nullptr);
      delete _buffer;
   }

   const VkAccelerationStructureKHR& AccelerationStructure::handle(void) const
   {
      return _handle;
   }

   uint64_t AccelerationStructure::deviceAddress(void) const
   {
      VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
      accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
      accelerationStructureDeviceAddressInfo.accelerationStructure = _handle;
      return vkGetAccelerationStructureDeviceAddressKHR(_device->vulkanDevice(), &accelerationStructureDeviceAddressInfo);
   }
}