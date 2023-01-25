/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

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
         , VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImageTiling imageTiling, int sampleCount);

      virtual ~StorageImage();
   public:
      //! get the view. this is lazily created
      virtual const VkImageView& vulkanImageView(void) const;
   protected:

      VkImageView _imageView;
      const VkImageUsageFlags _usageFlags;
   };
}

