#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace genesis
{
   class Device;
   class Image;

   class Texture
   {
   public:
      Texture(Image* image);
      virtual ~Texture();

   public:
      virtual const VkDescriptorImageInfo& descriptor(void) const;

      virtual const VkDescriptorImageInfo* descriptorPtr(void) const;

   protected:
      virtual void createSampler(void);

      virtual void createImageView(void);
   protected:
      const Image* _image;
      VkSampler sampler;
      VkImageView imageView;
      VkDescriptorImageInfo _descriptor;
   };
}
