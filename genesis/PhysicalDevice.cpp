#include "PhysicalDevice.h"
#include "Instance.h"

#include <iostream>

namespace genesis
{
   PhysicalDevice::PhysicalDevice(const Instance* instance, int deviceIndex)
   {
      _physicalDevice =  instance->physicalDevices()[deviceIndex];
      // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
      vkGetPhysicalDeviceProperties(_physicalDevice, &_deviceProperties);
      vkGetPhysicalDeviceFeatures(_physicalDevice, &_deviceFeatures);
      vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_deviceMemoryProperties);
   }
   
   PhysicalDevice::~PhysicalDevice()
   {

   }

   static std::string physicalDeviceTypeString(VkPhysicalDeviceType type)
   {
      switch (type)
      {
#define STR(r) case VK_PHYSICAL_DEVICE_TYPE_ ##r: return #r
         STR(OTHER);
         STR(INTEGRATED_GPU);
         STR(DISCRETE_GPU);
         STR(VIRTUAL_GPU);
         STR(CPU);
#undef STR
      default: return "UNKNOWN_DEVICE_TYPE";
      }
   }

   void PhysicalDevice::printDetails()
   {
      std::cout << "Device: " << _deviceProperties.deviceName << std::endl;
      std::cout << " Type: " << physicalDeviceTypeString(_deviceProperties.deviceType) << "\n";
      std::cout << " API: " << (_deviceProperties.apiVersion >> 22) << "." << ((_deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (_deviceProperties.apiVersion & 0xfff) << "\n";
   }

   const VkPhysicalDeviceProperties& PhysicalDevice::physicalDeviceProperties(void) const
   {
      return _deviceProperties;
   }

   VkPhysicalDevice PhysicalDevice::vulkanPhysicalDevice(void) const
   {
      return _physicalDevice;
   }

   const VkPhysicalDeviceFeatures& PhysicalDevice::physicalDeviceFeatures(void) const
   {
      return _physicalDeviceFeatures;
   }

   VkPhysicalDeviceFeatures& PhysicalDevice::enabledPhysicalDeviceFeatures(void)
   {
      return _enabledPhysicalDeviceFeatures;
   }
}