#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class ImageTransitions
   {
   public:
      ImageTransitions();
      virtual ~ImageTransitions();
   public:
      virtual void setImageLayout(
         VkCommandBuffer cmdbuffer,
         VkImage image,
         VkImageLayout oldImageLayout,
         VkImageLayout newImageLayout,
         VkImageSubresourceRange subresourceRange,
         VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

      virtual void setImageLayout(
         VkCommandBuffer cmdbuffer,
         VkImage image,
         VkImageAspectFlags aspectMask,
         VkImageLayout oldImageLayout,
         VkImageLayout newImageLayout,
         VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   };
}