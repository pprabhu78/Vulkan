#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

namespace genesis
{
   class Instance
   {
   public:
      Instance(const std::string& name, std::vector<std::string>& instanceExtensionsToEnable, uint32_t apiVersion, bool validation);
      virtual ~Instance();
   public:
      virtual VkInstance vulkanInstance(void) const;

      virtual VkResult creationStatus(void) const;

      virtual bool enumeratePhysicalDevices();

      virtual const std::vector<VkPhysicalDevice>& physicalDevices(void) const;
   protected:
      
      VkInstance _instance;

      std::vector<std::string> _supportedInstanceExtensions;

      const bool _validation;

      VkResult _createResult;

      std::vector<VkPhysicalDevice> _physicalDevices;
   };

}