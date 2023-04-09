#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"

namespace genesis
{
   void Device::initQueueFamilyIndices(VkQueueFlags requestedQueueTypes)
   {
      // Get queue family indices for the requested queue family types
      // Note that the indices may overlap depending on the implementation

      // Graphics queue
      if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
      {
         _queueFamilyIndices.graphics = _physicalDevice->queueFamilyIndexWithFlags(VK_QUEUE_GRAPHICS_BIT);
         VkDeviceQueueCreateInfo queueInfo{};
         queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
         queueInfo.queueFamilyIndex = _queueFamilyIndices.graphics;
         queueInfo.queueCount = 1;
         queueInfo.pQueuePriorities = &_defaultQueuePriority;
         _queueCreateInfos.push_back(queueInfo);
      }
      else
      {
         _queueFamilyIndices.graphics = 0;
      }

      // Dedicated compute queue
      if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
      {
         _queueFamilyIndices.compute = _physicalDevice->queueFamilyIndexWithFlags(VK_QUEUE_COMPUTE_BIT);
         if (_queueFamilyIndices.compute != _queueFamilyIndices.graphics)
         {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = _queueFamilyIndices.compute;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &_defaultQueuePriority;
            _queueCreateInfos.push_back(queueInfo);
         }
      }
      else
      {
         // Else we use the same queue
         _queueFamilyIndices.compute = _queueFamilyIndices.graphics;
      }

      // Dedicated transfer queue
      if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
      {
         _queueFamilyIndices.transfer = _physicalDevice->queueFamilyIndexWithFlags(VK_QUEUE_TRANSFER_BIT);
         if ((_queueFamilyIndices.transfer != _queueFamilyIndices.graphics) && (_queueFamilyIndices.transfer != _queueFamilyIndices.compute))
         {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = _queueFamilyIndices.transfer;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &_defaultQueuePriority;
            _queueCreateInfos.push_back(queueInfo);
         }
      }
      else
      {
         // Else we use the same queue
         _queueFamilyIndices.transfer = _queueFamilyIndices.graphics;
      }
   }

   Device::Device(PhysicalDevice* physicalDevice
      , void* pNextChain
      , const std::vector<const char*>& deviceExtensionsToEnable
      , const VkPhysicalDeviceFeatures& physicalDeviceFeaturesToEnable
      , bool useSwapChain, VkQueueFlags requestedQueueTypes)
      : _physicalDevice(physicalDevice)
      , _enableDebugMarkers(false)
      , _logicalDevice(0)
   {
      initQueueFamilyIndices(requestedQueueTypes);

      // Create the logical device representation
      std::vector<const char*> finalExtensions;
      for (const char* deviceExtensionRequestedToBeEnabled : deviceExtensionsToEnable)
      {
         if (_physicalDevice->extensionSupported(deviceExtensionRequestedToBeEnabled))
         {
            finalExtensions.push_back(deviceExtensionRequestedToBeEnabled);
         }
         else
         {
            std::cerr << deviceExtensionRequestedToBeEnabled << " not supported" << std::endl;
         }
      }

      if (useSwapChain)
      {
         if (_physicalDevice->extensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
         {
            // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
            finalExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
         }
         else
         {
            std::cerr << "swap chain extension not supported" << std::endl;
         }

      }

      VkDeviceCreateInfo deviceCreateInfo = {};
      deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(_queueCreateInfos.size());;
      deviceCreateInfo.pQueueCreateInfos = _queueCreateInfos.data();
      deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeaturesToEnable;

      // If a pNext(Chain) has been passed, we need to add it to the device creation info
      VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
      if (pNextChain) 
      {
         physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
         physicalDeviceFeatures2.features = physicalDeviceFeaturesToEnable;
         physicalDeviceFeatures2.pNext = pNextChain;
         deviceCreateInfo.pEnabledFeatures = nullptr;
         deviceCreateInfo.pNext = &physicalDeviceFeatures2;
      }

      // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
      // PPP: why does this not work? It does not seem to work for the original samples either
      //if (_physicalDevice->extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
      {
         _enableDebugMarkers = true;
      }

      if (finalExtensions.size() > 0)
      {
         deviceCreateInfo.enabledExtensionCount = (uint32_t)finalExtensions.size();
         deviceCreateInfo.ppEnabledExtensionNames = finalExtensions.data();
      }

      VkResult result = vkCreateDevice(_physicalDevice->vulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &_logicalDevice);
      if (result != VK_SUCCESS)
      {
         std::cout << "failed" << std::endl;
      }

      // Create a default command pool for graphics command buffers
      _graphicsCommandPool = createCommandPool(_queueFamilyIndices.graphics);

      // Get a graphics queue from the device
      vkGetDeviceQueue(_logicalDevice, _queueFamilyIndices.graphics, 0, &_graphicsQueue);

      _vulkanFunctions.initialize(this);
   }

   Device::~Device()
   {
      if (_graphicsCommandPool)
      {
         vkDestroyCommandPool(_logicalDevice, _graphicsCommandPool, nullptr);
      }
      if (_logicalDevice)
      {
         vkDestroyDevice(_logicalDevice, nullptr);
      }
   }

   VkDevice Device::vulkanDevice() const
   {
      return _logicalDevice;
   }

   const PhysicalDevice* Device::physicalDevice() const
   {
      return _physicalDevice;
   }

   VkCommandBuffer Device::createCommandBuffer(VkCommandBufferLevel level, bool begin)
   {
      VkCommandBuffer cmdBuffer;

      VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
      cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdBufAllocateInfo.commandPool = _graphicsCommandPool;
      cmdBufAllocateInfo.level = level;
      cmdBufAllocateInfo.commandBufferCount = 1;

      VK_CHECK_RESULT(vkAllocateCommandBuffers(_logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

      // If requested, also start the new command buffer
      if (begin)
      {
         VkCommandBufferBeginInfo cmdBufInfo = genesis::vkInitializers::commandBufferBeginInfo();
         VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
      }

      return cmdBuffer;
   }

   void Device::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
   {
      assert(commandBuffer != VK_NULL_HANDLE);
      if (commandBuffer == VK_NULL_HANDLE)
      {
         return;
      }

      VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

      VkSubmitInfo submitInfo = vkInitializers::submitInfo();
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer;

      // Create fence to ensure that the command buffer has finished executing
      VkFenceCreateInfo fenceCreateInfo = vkInitializers::fenceCreateInfo();
      VkFence fence;
      VK_CHECK_RESULT(vkCreateFence(_logicalDevice, &fenceCreateInfo, nullptr, &fence));

      // Submit to the queue
      VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));

      // Wait for the fence to signal that command buffer has finished executing
      VK_CHECK_RESULT(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

      vkDestroyFence(_logicalDevice, fence, nullptr);

      if (free)
      {
         vkFreeCommandBuffers(_logicalDevice, pool, 1, &commandBuffer);
      }
   }

   void Device::flushCommandBuffer(VkCommandBuffer commandBuffer)
   {
      flushCommandBuffer(commandBuffer, _graphicsQueue, _graphicsCommandPool);
   }

   VkCommandPool Device::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
   {
      VkCommandPoolCreateInfo cmdPoolInfo = {};
      cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
      cmdPoolInfo.flags = createFlags;
      VkCommandPool cmdPool;
      VK_CHECK_RESULT(vkCreateCommandPool(_logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
      return cmdPool;
   }

   VkQueue Device::graphicsQueue(void) const
   {
      return _graphicsQueue;
   }

   bool Device::enableDebugMarkers(void) const
   {
      return _enableDebugMarkers;
   }

   const vkExtensions& Device::extensions() const
   {
      return _vulkanFunctions;
   }

   SemaphoreHandle Device::semaphoreHandle(VkSemaphore semaphore) const
   {
#if _WIN32
      VkSemaphoreGetWin32HandleInfoKHR semaphoreGetWin32HandleInfoKHR{};
      semaphoreGetWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
      semaphoreGetWin32HandleInfoKHR.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
      semaphoreGetWin32HandleInfoKHR.semaphore = semaphore;

      HANDLE handle = 0;
      extensions().vkGetSemaphoreWin32HandleKHR(_logicalDevice, &semaphoreGetWin32HandleInfoKHR, &handle);
      return handle;
#else
      #error "Target platform not defined"
      return 0
#endif
   }

   MemoryHandle Device::memoryHandle(VkDeviceMemory memory) const
   {
#if _WIN32
      VkMemoryGetWin32HandleInfoKHR memoryGetWin32HandleInfoKHR{};
      memoryGetWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
      memoryGetWin32HandleInfoKHR.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
      memoryGetWin32HandleInfoKHR.memory = memory;

      HANDLE handle;
      extensions().vkGetMemoryWin32HandleKHR(_logicalDevice, &memoryGetWin32HandleInfoKHR, &handle);
      return handle;
#else
      #error "Target platform not defined"
      return 0
#endif
   }
}