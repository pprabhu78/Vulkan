#include "PhysicalDevice.h"
#include "Instance.h"

#include <iostream>
#include <cassert>

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

      // Queue family properties, used for setting up requested queues upon device creation
      uint32_t queueFamilyCount;
      vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);
      assert(queueFamilyCount > 0);
      _queueFamilyProperties.resize(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, _queueFamilyProperties.data());

      // Get list of supported extensions
      uint32_t extCount = 0;
      vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extCount, nullptr);
      if (extCount > 0)
      {
         std::vector<VkExtensionProperties> extensions(extCount);
         if (vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
         {
            for (auto ext : extensions)
            {
               _supportedExtensions.push_back(ext.extensionName);
            }
         }
      }
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


   uint32_t PhysicalDevice::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
   {
      // Iterate over all memory types available for the device used in this example
      for (uint32_t i = 0; i < _physicalDeviceMemoryProperties.memoryTypeCount; i++)
      {
         if ((typeBits & 1) == 1)
         {
            if ((_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
               return i;
            }
         }
         typeBits >>= 1;
      }

      throw "Could not find a suitable memory type!";
   }

   uint32_t PhysicalDevice::getQueueFamilyIndex(VkQueueFlagBits queueFlags) const
   {	
      // Dedicated queue for compute
      // Try to find a queue family index that supports compute but not graphics
      if (queueFlags & VK_QUEUE_COMPUTE_BIT)
      {
         for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
         {
            if ((_queueFamilyProperties[i].queueFlags & queueFlags) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
            {
               return i;
            }
         }
      }

      // Dedicated queue for transfer
      // Try to find a queue family index that supports transfer but not graphics and compute
      if (queueFlags & VK_QUEUE_TRANSFER_BIT)
      {
         for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
         {
            if ((_queueFamilyProperties[i].queueFlags & queueFlags) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
            {
               return i;
            }
         }
      }

      // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
      for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
      {
         if (_queueFamilyProperties[i].queueFlags & queueFlags)
         {
            return i;
         }
      }

      throw std::runtime_error("Could not find a matching queue family index");

   }

   bool PhysicalDevice::extensionSupported(const std::string& extension) const
   {
      return (std::find(_supportedExtensions.begin(), _supportedExtensions.end(), extension) != _supportedExtensions.end());
   }

   bool PhysicalDevice::getSupportedDepthFormat(VkFormat& depthFormat) const
   {
      // Since all depth formats may be optional, we need to find a suitable depth format to use
      // Start with the highest precision packed format
      std::vector<VkFormat> depthFormats = {
         VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D32_SFLOAT,
         VK_FORMAT_D24_UNORM_S8_UINT,
         VK_FORMAT_D16_UNORM_S8_UINT,
         VK_FORMAT_D16_UNORM
      };

      for (auto& format : depthFormats)
      {
         VkFormatProperties formatProps;
         vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &formatProps);
         // Format must support depth stencil attachment for optimal tiling
         if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
         {
            depthFormat = format;
            return true;
         }
      }

      return false;
   }
}