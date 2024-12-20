#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "VkExtensions.h"

namespace genesis
{
   class Buffer;
   class PhysicalDevice;
   class VulkanBuffer;

#if _WIN32
   typedef HANDLE SemaphoreHandle;
   typedef HANDLE MemoryHandle;
#else
   #error "Target platform not defined"
#endif

   class Device
   {
   public:
      Device(PhysicalDevice* physicalDevice
         , void* pNextChain
         , const std::vector<const char*>& deviceExtensionsToEnable
         , const VkPhysicalDeviceFeatures& physicalDeviceFeaturesToEnable
         , bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

      virtual ~Device();

   public:
      virtual VkDevice vulkanDevice() const;

      virtual const PhysicalDevice* physicalDevice() const;

      virtual VkQueue graphicsQueue(void) const;

      virtual VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);

      //! use specified queue
      virtual void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);

      //! graphics queue
      virtual void flushCommandBuffer(VkCommandBuffer commandBuffer);

      virtual VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

      virtual bool enableDebugMarkers(void) const;

      virtual const vkExtensions& extensions() const;
      
      //! handle to a semaphore
      virtual SemaphoreHandle semaphoreHandle(VkSemaphore semaphore) const;

      //! handle to memory
      virtual MemoryHandle memoryHandle(VkDeviceMemory memory) const;

   protected:
      virtual void initQueueFamilyIndices(VkQueueFlags requestedQueueTypes);
   public:
      const PhysicalDevice* _physicalDevice;

      VkDevice _logicalDevice;

      VkCommandPool _graphicsCommandPool;
      VkQueue _graphicsQueue;

      std::vector<VkDeviceQueueCreateInfo> _queueCreateInfos{};

      struct
      {
         uint32_t graphics;
         uint32_t compute;
         uint32_t transfer;
      } _queueFamilyIndices;

      bool _enableDebugMarkers;

      const float _defaultQueuePriority = 0.0f;

      vkExtensions _vulkanFunctions;
   };
}