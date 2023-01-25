/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "StorageImage.h"
#include "VulkanInitializers.h"
#include "Device.h"
#include "ImageTransitions.h"
#include "VulkanDebug.h"

namespace genesis
{
   StorageImage::StorageImage(Device* device, VkFormat format, int width, int height
      , VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImageTiling imageTiling, int sampleCount)
      : Image(device)
      , _imageView(0)
      , _usageFlags(usageFlags)
   {
      _format = format;
      _width = width;
      _height = height;
      _numMipMapLevels = 1;

      allocateImageAndMemory(usageFlags, memoryPropertyFlags, imageTiling, 1, sampleCount);
   }

   StorageImage::~StorageImage()
   {
      vkDestroyImageView(_device->vulkanDevice(), _imageView, nullptr);
   }

   const VkImageView& StorageImage::vulkanImageView(void) const
   {
      if (_imageView)
      {
         return _imageView;
      }

      VkImageViewCreateInfo imageViewCreateInfo = genesis::VulkanInitializers::imageViewCreateInfo();
      imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewCreateInfo.format = _format;
      imageViewCreateInfo.subresourceRange = {};
      if (_usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      {
         imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
         if (_format >= VK_FORMAT_D16_UNORM_S8_UINT) {
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
         }
      }
      else
      {
         imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      }
         
      imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
      imageViewCreateInfo.subresourceRange.levelCount = 1;
      imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
      imageViewCreateInfo.subresourceRange.layerCount = 1;
      imageViewCreateInfo.image = _image;
      StorageImage* nonConst = const_cast<StorageImage*>(this);
      VK_CHECK_RESULT(vkCreateImageView(_device->vulkanDevice(), &imageViewCreateInfo, nullptr, &(nonConst->_imageView)));

      return _imageView;
   }
}