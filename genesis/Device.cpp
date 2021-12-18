#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"


namespace genesis
{
   Device::Device(PhysicalDevice* physicalDevice, VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool)
      : _logicalDevice(logicalDevice)
      , _queue(graphicsQueue)
      , _commandPool(commandPool)
      , _physicalDevice(physicalDevice)
   {
      
   }

   Device::~Device()
   {

   }

   VkDevice Device::vulkanDevice() const
   {
      return _logicalDevice;
   }

   const PhysicalDevice* Device::physicalDevice() const
   {
      return _physicalDevice;
   }

   VkCommandBuffer Device::getCommandBuffer(bool begin)
   {
      VkCommandBuffer cmdBuffer;

      VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
      cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdBufAllocateInfo.commandPool = _commandPool;
      cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmdBufAllocateInfo.commandBufferCount = 1;

      VK_CHECK_RESULT(vkAllocateCommandBuffers(_logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

      // If requested, also start the new command buffer
      if (begin)
      {
         VkCommandBufferBeginInfo cmdBufInfo = genesis::VulkanInitializers::commandBufferBeginInfo();
         VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
      }

      return cmdBuffer;
   }
   void Device::flushCommandBuffer(VkCommandBuffer commandBuffer)
   {
      assert(commandBuffer != VK_NULL_HANDLE);

      VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer;

      // Create fence to ensure that the command buffer has finished executing
      VkFenceCreateInfo fenceCreateInfo = {};
      fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceCreateInfo.flags = 0;
      VkFence fence;
      VK_CHECK_RESULT(vkCreateFence(_logicalDevice, &fenceCreateInfo, nullptr, &fence));

      // Submit to the queue
      VK_CHECK_RESULT(vkQueueSubmit(_queue, 1, &submitInfo, fence));
      // Wait for the fence to signal that command buffer has finished executing
      VK_CHECK_RESULT(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

      vkDestroyFence(_logicalDevice, fence, nullptr);
      vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &commandBuffer);
   }

   uint32_t Device::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
   {
      const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties = _physicalDevice->physicalDeviceMemoryProperties();

      // Iterate over all memory types available for the device used in this example
      for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
      {
         if ((typeBits & 1) == 1)
         {
            if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
               return i;
            }
         }
         typeBits >>= 1;
      }

      throw "Could not find a suitable memory type!";
   }
}