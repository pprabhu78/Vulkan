#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Device;

   class StorageImage
   {
   public:
      StorageImage(Device* device, VkFormat format, int width, int height);
      virtual ~StorageImage();
   public:
      const VkImageView& vulkanImageView(void) const;
      const VkImage& vulkanImage(void) const;
   protected:
      VkDeviceMemory _memory;
      VkImage _image;
      VkImageView _view;
      VkFormat _format;

      Device* _device;
   };
}

