#include "ScreenShotUtility.h"
#include "ImageTransitions.h"
#include "StorageImage.h"
#include "Device.h"

#include <iostream>
#include <fstream>

namespace genesis
{
   ScreenShotUtility::ScreenShotUtility(Device* device)
      : _device(device)
   {
      // nothing to do
   }

   ScreenShotUtility::~ScreenShotUtility()
   {
      // nothing to do
   }

   void ScreenShotUtility::takeScreenShot(const std::string& fileName
      , VkImage swapChainCurrentImage, VkFormat swapChainColorFormat
      , int swapChainWidth, int swapChainHeight
   )
   {
      bool supportsBlit = false;
      VkFormatProperties formatProps;

      // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
      vkGetPhysicalDeviceFormatProperties(_device->physicalDevice(), swapChainColorFormat, &formatProps);
      if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
         std::cout << "Device does not support blitting from optimal tiled images, using copy instead of blit!" << std::endl;
         supportsBlit = false;
      }

      // Check if the device supports blitting to linear images
      vkGetPhysicalDeviceFormatProperties(_device->physicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
      if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
         std::cout << "Device does not support blitting to linear tiled images, using copy instead of blit!" << std::endl;
         supportsBlit = false;
      }

      StorageImage* destinationStorageImage = new genesis::StorageImage(_device
         , VK_FORMAT_R8G8B8A8_UNORM, swapChainWidth, swapChainHeight
         , VK_IMAGE_USAGE_TRANSFER_DST_BIT
         , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
         , VK_IMAGE_TILING_LINEAR
      );

      VkImage srcImage = swapChainCurrentImage;
      VkImage dstImage = destinationStorageImage->vulkanImage();

      VkCommandBuffer commandBuffer = _device->getCommandBuffer(true);

      genesis::ImageTransitions transitions;

      VkImageSubresourceRange subResourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

      transitions.setImageLayout(commandBuffer, srcImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subResourceRange);

      transitions.setImageLayout(commandBuffer, dstImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subResourceRange);

      // blit does filtering and down sampling
      // here source and destination sizes are the same, so it doesn't matter
      // but blit is still preferred.
      if (supportsBlit)
      {
         VkOffset3D blitSize{ (int32_t)swapChainWidth, (int32_t)swapChainHeight, (int32_t)1 };
         VkImageBlit imageBlitRegion{};
         imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         imageBlitRegion.srcSubresource.layerCount = 1;
         imageBlitRegion.srcOffsets[1] = blitSize;
         imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         imageBlitRegion.dstSubresource.layerCount = 1;
         imageBlitRegion.dstOffsets[1] = blitSize;

         vkCmdBlitImage(commandBuffer
            , srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            , dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            , 1, &imageBlitRegion, VK_FILTER_NEAREST);
      }
      else
      {
         // Otherwise use image copy (requires us to manually flip components)
         VkImageCopy imageCopyRegion{};
         imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         imageCopyRegion.srcSubresource.layerCount = 1;
         imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         imageCopyRegion.dstSubresource.layerCount = 1;
         imageCopyRegion.extent.width = swapChainWidth;
         imageCopyRegion.extent.height = swapChainHeight;
         imageCopyRegion.extent.depth = 1;

         // Issue the copy command
         vkCmdCopyImage(
            commandBuffer
            , srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            , dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
      }

      transitions.setImageLayout(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subResourceRange);

      transitions.setImageLayout(commandBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, subResourceRange);

      _device->flushCommandBuffer(commandBuffer);

      // Get layout of the image (including row pitch)
      VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
      VkSubresourceLayout subResourceLayout;
      vkGetImageSubresourceLayout(_device->vulkanDevice(), dstImage, &subResource, &subResourceLayout);

      // Map image memory so we can start copying from it
      const char* data;
      vkMapMemory(_device->vulkanDevice(), destinationStorageImage->vulkanDeviceMemory(), 0, VK_WHOLE_SIZE, 0, (void**)&data);
      data += subResourceLayout.offset;

      std::ofstream file(fileName.c_str(), std::ios::out | std::ios::binary);

      // ppm header
      file << "P6\n" << swapChainWidth << "\n" << swapChainHeight << "\n" << 255 << "\n";

      // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
      bool colorSwizzle = false;
      // Check if source is BGR
      // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
      if (!supportsBlit)
      {
         std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
         colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChainColorFormat) != formatsBGR.end());
      }

      // ppm binary pixel data
      for (uint32_t y = 0; y < (uint32_t)swapChainHeight; y++)
      {
         unsigned int* row = (unsigned int*)data;
         for (uint32_t x = 0; x < (uint32_t)swapChainWidth; x++)
         {
            if (colorSwizzle)
            {
               file.write((char*)row + 2, 1);
               file.write((char*)row + 1, 1);
               file.write((char*)row, 1);
            }
            else
            {
               file.write((char*)row, 3);
            }
            row++;
         }
         data += subResourceLayout.rowPitch;
      }
      file.close();

      delete destinationStorageImage;
   }
}