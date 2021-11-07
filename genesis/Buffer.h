#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   enum BufferType
   {
      BT_STAGING = 0,
      BT_VERTEX_BUFFER,
      BT_INDEX_BUFFER,
      BT_UBO,
      BT_SBO,
      BT_INDIRECT_BUFFER
   };

   class Device;

   //! internal structure for buffer and memory
   //! associated with it.
   //! can be a staging buffer or a device local buffer
   class VulkanBuffer
   {
   public:
      VulkanBuffer(Device* device, BufferType bufferType, int sizeInBytes);
      virtual ~VulkanBuffer();

   public:
      virtual VkBuffer vulkanBuffer(void) const;
   public:
      VkBuffer _buffer;
      VkDeviceMemory _deviceMemory;

      Device* _device;
   };

   class Buffer
   {
   public:
      //! constructor
      //! staging means create a staging buffer
      //! stagingOnly means create only the staging buffer
      Buffer(Device* device, BufferType bufferType, int sizeInBytes, bool staging);

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

      virtual VkDescriptorBufferInfo descriptor(void) const;

   protected:

      VulkanBuffer* _stagingBuffer;
      VulkanBuffer* _buffer;

      Device* _device;

      int _sizeInBytes;
   };

}