#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace genesis
{
   class Device;

   enum BufferType
   {
      BT_STAGING = 0,
      BT_VERTEX_BUFFER,
      BT_INDEX_BUFFER,
      BT_UBO,
      BT_SBO,
      BT_INDIRECT_BUFFER
   };

   //! internal structure for buffer and memory
   //! associated with it.
   //! can be a staging buffer or a device local buffer
   class VulkanBuffer
   {
   public:
      VulkanBuffer(Device* device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize sizeInBytes, void* data = 0);
      virtual ~VulkanBuffer();
   public:
      virtual VkBuffer vulkanBuffer(void) const;

      virtual VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
      virtual void unmap();
      virtual VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

      virtual uint64_t bufferAddress() const;
   public:
      VkBuffer _buffer;
      VkDeviceMemory _deviceMemory;

      Device* _device;

      void* _mapped = nullptr;
   };

   class Buffer
   {
   public:
      //! constructor
      //! staging means create a staging buffer
      //! stagingOnly means create only the staging buffer
      Buffer(Device* device, BufferType bufferType, int sizeInBytes, bool staging, VkBufferUsageFlags additionalFlags = 0, const std::string& name = "");

      //! destructor
      virtual ~Buffer(void);

   public:
      virtual void* Buffer::stagingBuffer(void);

      //! update the actual buffer from the staging buffer
      //! destroyStaging is whether to delete the staging buffer after the 
      //! update has happened
      virtual bool syncToGpu(bool destroyStaging);

      //! access the internal vulkan buffer
      virtual VkBuffer vulkanBuffer(void) const;

      virtual int sizeInBytes(void) const;

      virtual const VkDescriptorBufferInfo& descriptor(void) const;

      virtual const VkDescriptorBufferInfo* descriptorPtr(void) const;

      virtual uint64_t bufferAddress() const;

   protected:

      VulkanBuffer* _stagingBuffer;
      VulkanBuffer* _buffer;

      VkDescriptorBufferInfo _descriptor;

      Device* _device;

      int _sizeInBytes;
   };

}