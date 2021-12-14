#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Buffer;

   class Device
   {
   public:
      Device(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);
      virtual ~Device();

      VkDevice vulkanDevice() const;

      VkPhysicalDevice physicalDevice() const;

      virtual VkCommandBuffer getCommandBuffer(bool begin);
      virtual void flushCommandBuffer(VkCommandBuffer commandBuffer);

      virtual uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

   public:
      const VkPhysicalDevice _physicalDevice;

      VkDevice _logicalDevice;

      VkCommandPool _commandPool;
      VkQueue _queue;

      // Stores physical device properties (for e.g. checking device limits)
      VkPhysicalDeviceProperties _physicalDeviceProperties;

      // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
      VkPhysicalDeviceFeatures _physicalDeviceFeatures;

      // Stores all available memory (type) properties for the physical device
      VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;
   };
}