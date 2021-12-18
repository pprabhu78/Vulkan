#include "PhysicalDevice.h"
#include "Instance.h"

#include <iostream>

namespace genesis
{
   PhysicalDevice::PhysicalDevice(const Instance* instance, int deviceIndex
      , const std::vector<const char*>& enabledPhysicalDeviceExtensions)
      : _enabledPhysicalDeviceExtensions(enabledPhysicalDeviceExtensions)
   {
      _physicalDevice =  instance->physicalDevices()[deviceIndex];
      // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
      vkGetPhysicalDeviceProperties(_physicalDevice, &_physicalDeviceProperties);
      vkGetPhysicalDeviceFeatures(_physicalDevice, &_physicalDeviceFeatures);
      vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_physicalDeviceMemoryProperties);
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
      std::cout << "Device: " << _physicalDeviceProperties.deviceName << std::endl;
      std::cout << " Type: " << physicalDeviceTypeString(_physicalDeviceProperties.deviceType) << "\n";
      std::cout << " API: " << (_physicalDeviceProperties.apiVersion >> 22) << "." << ((_physicalDeviceProperties.apiVersion >> 12) & 0x3ff) << "." << (_physicalDeviceProperties.apiVersion & 0xfff) << "\n";
   }

   const VkPhysicalDeviceProperties& PhysicalDevice::physicalDeviceProperties(void) const
   {
      return _physicalDeviceProperties;
   }

   VkPhysicalDevice PhysicalDevice::vulkanPhysicalDevice(void) const
   {
      return _physicalDevice;
   }

   const VkPhysicalDeviceFeatures& PhysicalDevice::enabledPhysicalDeviceFeatures(void) const
   {
      return _enabledPhysicalDeviceFeatures;
   }
   VkPhysicalDeviceFeatures& PhysicalDevice::enabledPhysicalDeviceFeatures(void)
   {
      return _enabledPhysicalDeviceFeatures;
   }
   
   const std::vector<const char*>& PhysicalDevice::enabledPhysicalDeviceExtensions(void) const
   {
      return _enabledPhysicalDeviceExtensions;
   }

   const VkPhysicalDeviceFeatures& PhysicalDevice::physicalDeviceFeatures(void) const
   {
      return _physicalDeviceFeatures;
   }

   const VkPhysicalDeviceMemoryProperties& PhysicalDevice::physicalDeviceMemoryProperties(void) const
   {
      return _physicalDeviceMemoryProperties;
   }
}