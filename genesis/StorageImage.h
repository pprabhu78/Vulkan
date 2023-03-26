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

   //! An image directly made with flags for usage, memory type, tiling and sample count
   //! It is used for storing results of some intermediate rendering (e.g. ray tracing), blit screenshots, etc
   class StorageImage : public Image
   {
   public:
      //! constructor
      StorageImage(Device* device, VkFormat format, int width, int height
         , VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImageTiling imageTiling, int sampleCount, bool exportMemory);

      //! destructor
      virtual ~StorageImage();
   private:
      //! not allowed
      StorageImage() = delete;
      //! not allowed
      StorageImage(const StorageImage& rhs) = delete;
      //! not allowed
      StorageImage& operator=(const StorageImage& rhs) = delete;
   public:
      //! get the view. this is lazily created
      virtual const VkImageView& vulkanImageView(void) const;
   protected:

      VkImageView _imageView = VK_NULL_HANDLE;
      const VkImageUsageFlags _usageFlags = 0;
   };
}

