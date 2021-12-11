#include "Texture.h"
#include "Image.h"
#include "Device.h"

#include "VulkanInitializers.h"
#include "VulkanDebug.h"

namespace genesis
{

   Texture::Texture(Image* image)
      : _image(image)
   {
      createSampler();
      createImageView();
   }

   Texture::~Texture()
   {
      VkDevice vulkanDevice = _image->device()->vulkanDevice();
      vkDestroyImageView(vulkanDevice, imageView, nullptr);
      vkDestroySampler(vulkanDevice, sampler, nullptr);
   }

   void Texture::createSampler()
   {
      VkSamplerCreateInfo samplerInfo = VulkanInitializers::samplerCreateInfo();
      samplerInfo.magFilter = VK_FILTER_LINEAR;
      samplerInfo.minFilter = VK_FILTER_LINEAR;
      samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.mipLodBias = 0.0f;
      samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
      samplerInfo.minLod = 0.0f;
      // Set max level-of-detail to mip level count of the texture
      samplerInfo.maxLod = (float)(_image->numMipMapLevels());

      samplerInfo.maxAnisotropy = 1.0;
      samplerInfo.anisotropyEnable = VK_FALSE;
      samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

      VK_CHECK_RESULT(vkCreateSampler(_image->device()->vulkanDevice(), &samplerInfo, nullptr, &sampler));
   }

   void Texture::createImageView()
   {
      VkImageViewCreateInfo imageViewInfo = VulkanInitializers::imageViewCreateInfo();
      imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewInfo.format = _image->vulkanFormat();
      imageViewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
      // The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
      // It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
      imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageViewInfo.subresourceRange.baseMipLevel = 0;
      imageViewInfo.subresourceRange.baseArrayLayer = 0;
      imageViewInfo.subresourceRange.layerCount = 1;

      imageViewInfo.subresourceRange.levelCount = _image->numMipMapLevels();
      // The view will be based on the texture's image
      imageViewInfo.image = _image->vulkanImage();
      VK_CHECK_RESULT(vkCreateImageView(_image->device()->vulkanDevice(), &imageViewInfo, nullptr, &imageView));
   }

   VkDescriptorImageInfo Texture::descriptor() const
   {
      return VkDescriptorImageInfo{ sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
   }
}