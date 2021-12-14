#include "StorageImage.h"
#include "VulkanInitializers.h"
#include "Device.h"
#include "ImageTransitions.h"
#include "VulkanDebug.h"

namespace genesis
{
   StorageImage::StorageImage(Device* device, VkFormat format, int width, int height
      , VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImageTiling imageTiling)
      : Image(device)
      , _imageView(0)
   {
      _format = format;
      _width = width;
      _height = height;
      _numMipMapLevels = 1;

      allocateImageAndMemory(usageFlags, memoryPropertyFlags, imageTiling);
   }

   StorageImage::~StorageImage()
   {
      vkDestroyImageView(_device->vulkanDevice(), _imageView, nullptr);
   }

   const VkImageView& StorageImage::vulkanImageView(void) const
   {
      if (_imageView == 0)
      {
         VkImageViewCreateInfo colorImageView = genesis::VulkanInitializers::imageViewCreateInfo();
         colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
         colorImageView.format = _format;
         colorImageView.subresourceRange = {};
         colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         colorImageView.subresourceRange.baseMipLevel = 0;
         colorImageView.subresourceRange.levelCount = 1;
         colorImageView.subresourceRange.baseArrayLayer = 0;
         colorImageView.subresourceRange.layerCount = 1;
         colorImageView.image = _image;
         StorageImage* nonConst = const_cast<StorageImage*>(this);
         VK_CHECK_RESULT(vkCreateImageView(_device->vulkanDevice(), &colorImageView, nullptr, &(nonConst->_imageView)));
      }

      return _imageView;
   }
}