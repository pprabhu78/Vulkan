#include "AccelerationStructure.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "VulkanFunctions.h"
#include "Buffer.h"

#include <iostream>
#include <sstream>

namespace genesis
{
   std::atomic<int> AccelerationStructure::s_totalCount = 0;

   AccelerationStructure::AccelerationStructure(Device* device, VkAccelerationStructureTypeKHR type, uint64_t sizeInBytes, const std::string& incomingName)
      : _device(device)
      , _type(type)
   {
      
      std::string actualName;
      if (actualName.empty())
      {
         std::stringstream ss;
         ss << "AccelerationStructure[" << s_totalCount << "]";
         actualName = ss.str();
      }
      else
      {
         actualName = incomingName;
      }

      const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
      const VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      _buffer = new VulkanBuffer(_device, bufferUsageFlags, memoryPropertyFlags, sizeInBytes, nullptr, actualName + ":buffer");

      // Acceleration structure
      VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
      accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
      accelerationStructureCreateInfo.buffer = _buffer->vulkanBuffer();
      accelerationStructureCreateInfo.size = sizeInBytes;
      accelerationStructureCreateInfo.type = type;
      vkCreateAccelerationStructureKHR(_device->vulkanDevice(), &accelerationStructureCreateInfo, nullptr, &_handle);

      ++s_totalCount;
   }

   AccelerationStructure::~AccelerationStructure()
   {
      vkDestroyAccelerationStructureKHR(_device->vulkanDevice(), _handle, nullptr);
      delete _buffer;

      --s_totalCount;
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