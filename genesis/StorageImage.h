#pragma once

#include <vulkan/vulkan.h>

#include "Image.h"

namespace genesis
{
   class Device;

   class StorageImage : public Image
   {
   public:
      StorageImage(Device* device, VkFormat format, int width, int height);
      virtual ~StorageImage();
   public:
      virtual const VkImageView& vulkanImageView(void) const;
   protected:

      VkImageView _view;
   };
}

