#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Buffer;
   class PhysicalDevice;

   class Device
   {
   public:
      Device(PhysicalDevice* physicalDevice, VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);
      virtual ~Device();

      VkDevice vulkanDevice() const;

      const PhysicalDevice* physicalDevice() const;

      virtual VkCommandBuffer getCommandBuffer(bool begin);
      virtual void flushCommandBuffer(VkCommandBuffer commandBuffer);

      virtual uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

   public:
      const PhysicalDevice* _physicalDevice;

      VkDevice _logicalDevice;

      VkCommandPool _commandPool;
      VkQueue _queue;
   };
}