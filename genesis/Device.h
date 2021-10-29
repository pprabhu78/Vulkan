#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Buffer;

   class Device
   {
   public:
      Device(VkDevice _logicalDevice, VkQueue _graphicsQueue, VkCommandPool _commandPool, VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties);
      virtual ~Device();

      VkDevice vulkanDevice() const;

      virtual VkCommandBuffer getCommandBuffer(bool begin);
      virtual void flushCommandBuffer(VkCommandBuffer commandBuffer);

      uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

   public:
      VkDevice logicalDevice;
      VkCommandPool commandPool;
      VkQueue queue;
      VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
   };
}