#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Device;

   class RenderPass
   {
   public:
      RenderPass(Device* device, VkFormat colorFormat, VkFormat depthFormat, VkAttachmentLoadOp colorLoadOp);
      virtual ~RenderPass(void);

      VkRenderPass vulkanRenderPass(void) const;
   protected:

      VkRenderPass _renderPass;
      Device* _device;

   };
}