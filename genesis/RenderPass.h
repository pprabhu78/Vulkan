/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Device;

   class RenderPass
   {
   public:
      //! constructor
      RenderPass(Device* device, VkFormat colorFormat, VkFormat depthFormat, VkAttachmentLoadOp colorLoadOp, int sampleCount);
      //! destructor
      virtual ~RenderPass(void);
   private:
      //! not allowed
      RenderPass() = delete;
      //! not allowed
      RenderPass(const RenderPass& rhs) = delete;
      //! not allowed
      RenderPass& operator=(const RenderPass& rhs) = delete;
   public:
      //! access internal render pass
      VkRenderPass vulkanRenderPass(void) const;
   protected:
      //! create regular pass for when sample count is 1
      virtual void createRegular(VkFormat colorFormat, VkFormat depthFormat, VkAttachmentLoadOp colorLoadOp);
      //! create multi-sampled pass for when sample count is > 1
      virtual void createMultiSampled(VkFormat colorFormat, VkFormat depthFormat, VkAttachmentLoadOp colorLoadOp, int sampleCount);
   protected:
      VkRenderPass _renderPass = VK_NULL_HANDLE;
      Device* _device = nullptr;
   };
}