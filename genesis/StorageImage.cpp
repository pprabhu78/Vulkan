#include "StorageImage.h"
#include "VulkanInitializers.h"
#include "Device.h"
#include "ImageTransitions.h"
#include "VulkanDebug.h"

namespace genesis
{
   StorageImage::StorageImage(Device* device, VkFormat format, int width, int height)
      : Image(device)
   {
      _format = format;
      _width = width;
      _height = height;
      _numMipMapLevels = 1;

      allocateImageAndMemory(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

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
      VK_CHECK_RESULT(vkCreateImageView(_device->vulkanDevice(), &colorImageView, nullptr, &_view));

      VkCommandBuffer commandBuffer = _device->getCommandBuffer(true);

      ImageTransitions transition;
      transition.setImageLayout(commandBuffer, _image,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_GENERAL,
         { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
      _device->flushCommandBuffer(commandBuffer);
   }

   StorageImage::~StorageImage()
   {
      vkDestroyImageView(_device->vulkanDevice(), _view, nullptr);
   }

   const VkImageView& StorageImage::vulkanImageView(void) const
   {
      return _view;
   }
}