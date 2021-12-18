#pragma once
#include "Instance.h"

namespace genesis
{
   class Instance;

   class PhysicalDevice
   {
   public:
      PhysicalDevice(const Instance* instance, int deviceIndex, const std::vector<const char*>& enabledPhysicalDeviceExtensions);
      virtual ~PhysicalDevice();
   public:
      virtual void printDetails(void);

      virtual VkPhysicalDevice vulkanPhysicalDevice(void) const;

      //! all the properties (initialized in constructor)
      virtual const VkPhysicalDeviceProperties& physicalDeviceProperties(void) const;

      //! All the features supported (initialized in constructor)
      virtual const VkPhysicalDeviceFeatures& physicalDeviceFeatures(void) const;

      //! All the memory properties supported (initialized in constructor)
      virtual const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties(void) const;


      //! All the features enabled (non-const). Used to set
      virtual VkPhysicalDeviceFeatures& enabledPhysicalDeviceFeatures(void);

      //! All the features enabled (const). Used to query after setting
      virtual const VkPhysicalDeviceFeatures& enabledPhysicalDeviceFeatures(void) const;

      //! All the extensions enabled (const)
      virtual const std::vector<const char*>& enabledPhysicalDeviceExtensions(void) const;
      
   protected:
      // Stores physical device properties (for e.g. checking device limits)
      VkPhysicalDeviceProperties _physicalDeviceProperties;

      // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
      VkPhysicalDeviceFeatures _physicalDeviceFeatures;

      // Stores all available memory (type) properties for the physical device
      VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;

      VkPhysicalDevice _physicalDevice;

      VkPhysicalDeviceFeatures _enabledPhysicalDeviceFeatures{};

      /** @brief Set of device extensions to be enabled */
      std::vector<const char*> _enabledPhysicalDeviceExtensions;
   };
}

