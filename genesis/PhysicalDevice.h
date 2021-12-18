#pragma once
#include "Instance.h"

namespace genesis
{
   class Instance;

   class PhysicalDevice
   {
   public:
      PhysicalDevice(const Instance* instance, int deviceIndex);
      virtual ~PhysicalDevice();
   public:
      virtual void printDetails(void);

      virtual const VkPhysicalDeviceProperties& physicalDeviceProperties(void) const;

      virtual VkPhysicalDevice vulkanPhysicalDevice(void) const;

      virtual const VkPhysicalDeviceFeatures& physicalDeviceFeatures(void) const;

      virtual VkPhysicalDeviceFeatures& enabledPhysicalDeviceFeatures(void);
      
   protected:
      // Stores physical device properties (for e.g. checking device limits)
      VkPhysicalDeviceProperties _deviceProperties;

      // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
      VkPhysicalDeviceFeatures _deviceFeatures;

      // Stores all available memory (type) properties for the physical device
      VkPhysicalDeviceMemoryProperties _deviceMemoryProperties;

      VkPhysicalDevice _physicalDevice;

      /** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
      const VkPhysicalDeviceFeatures _physicalDeviceFeatures{};

      VkPhysicalDeviceFeatures _enabledPhysicalDeviceFeatures{};
   };
}

