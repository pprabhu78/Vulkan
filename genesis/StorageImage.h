#pragma once

#include <vulkan/vulkan.h>

#include "Image.h"

namespace genesis
{
   class Device;

   //! generic storage image
   class StorageImage : public Image
   {
   public:
      StorageImage(Device* device, VkFormat format, int width, int height
         , VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImageTiling imageTiling);

      virtual ~StorageImage();
   public:
      //! get the view. this is lazily created
      virtual const VkImageView& vulkanImageView(void) const;
   protected:

      VkImageView _imageView;
   };
}

