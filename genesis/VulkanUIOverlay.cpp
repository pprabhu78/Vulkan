
/*
* UI overlay class using ImGui
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanUIOverlay.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanInitializers.h"
#include "Buffer.h"
#include "ImageTransitions.h"
#include "VulkanDebug.h"
#include "Shader.h"

namespace genesis
{
   UIOverlay::UIOverlay()
   {
      // Init ImGui
      ImGui::CreateContext();
      // Color scheme
      ImGuiStyle& style = ImGui::GetStyle();

      ImGui::StyleColorsDark();
      style.ScaleAllSizes(2);


      // Dimensions
      ImGuiIO& io = ImGui::GetIO();
      io.FontGlobalScale = scale;
   }

   UIOverlay::~UIOverlay() { }

   const std::string getAssetPath()
   {
      return VK_EXAMPLE_DATA_DIR;
   }

   /** Prepare all vulkan resources required to render the UI overlay */
   void UIOverlay::prepareResources()
   {
      ImGuiIO& io = ImGui::GetIO();

      // Create font texture
      unsigned char* fontData;
      int texWidth, texHeight;

      const std::string filename = getAssetPath() + "Roboto-Medium.ttf";
      io.Fonts->AddFontFromFileTTF(filename.c_str(), 24);

      io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
      VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

      // Create target image for copy
      VkImageCreateInfo imageInfo = VulkanInitializers::imageCreateInfo();
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
      imageInfo.extent.width = texWidth;
      imageInfo.extent.height = texHeight;
      imageInfo.extent.depth = 1;
      imageInfo.mipLevels = 1;
      imageInfo.arrayLayers = 1;
      imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      VK_CHECK_RESULT(vkCreateImage(device->vulkanDevice(), &imageInfo, nullptr, &fontImage));
      VkMemoryRequirements memReqs;
      vkGetImageMemoryRequirements(device->vulkanDevice(), fontImage, &memReqs);
      VkMemoryAllocateInfo memAllocInfo = VulkanInitializers::memoryAllocateInfo();
      memAllocInfo.allocationSize = memReqs.size;
      memAllocInfo.memoryTypeIndex = device->physicalDevice()->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(device->vulkanDevice(), &memAllocInfo, nullptr, &fontMemory));
      VK_CHECK_RESULT(vkBindImageMemory(device->vulkanDevice(), fontImage, fontMemory, 0));

      // Image view
      VkImageViewCreateInfo viewInfo = VulkanInitializers::imageViewCreateInfo();
      viewInfo.image = fontImage;
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.layerCount = 1;
      VK_CHECK_RESULT(vkCreateImageView(device->vulkanDevice(), &viewInfo, nullptr, &fontView));

      // Staging buffers for font data upload
      VulkanBuffer* stagingBuffer = new VulkanBuffer(device
         , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
         , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
         , uploadSize);

      stagingBuffer->map();
      memcpy(stagingBuffer->_mapped, fontData, uploadSize);
      stagingBuffer->unmap();

      // Copy buffer data to font image
      VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

      // Prepare for transfer
      ImageTransitions transitions;
      transitions.setImageLayout(
         copyCmd,
         fontImage,
         VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_PIPELINE_STAGE_HOST_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

      // Copy
      VkBufferImageCopy bufferCopyRegion = {};
      bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      bufferCopyRegion.imageSubresource.layerCount = 1;
      bufferCopyRegion.imageExtent.width = texWidth;
      bufferCopyRegion.imageExtent.height = texHeight;
      bufferCopyRegion.imageExtent.depth = 1;

      vkCmdCopyBufferToImage(
         copyCmd,
         stagingBuffer->_buffer,
         fontImage,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1,
         &bufferCopyRegion
      );

      // Prepare for shader read
      transitions.setImageLayout(
         copyCmd,
         fontImage,
         VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

      device->flushCommandBuffer(copyCmd);

      delete stagingBuffer;

      // Font texture Sampler
      VkSamplerCreateInfo samplerInfo = VulkanInitializers::samplerCreateInfo();
      samplerInfo.magFilter = VK_FILTER_LINEAR;
      samplerInfo.minFilter = VK_FILTER_LINEAR;
      samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
      VK_CHECK_RESULT(vkCreateSampler(device->vulkanDevice(), &samplerInfo, nullptr, &sampler));

      // Descriptor pool
      std::vector<VkDescriptorPoolSize> poolSizes = {
      VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
      };
      VkDescriptorPoolCreateInfo descriptorPoolInfo = VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 2);
      VK_CHECK_RESULT(vkCreateDescriptorPool(device->vulkanDevice(), &descriptorPoolInfo, nullptr, &descriptorPool));

      // Descriptor set layout
      std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
      };
      VkDescriptorSetLayoutCreateInfo descriptorLayout = VulkanInitializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->vulkanDevice(), &descriptorLayout, nullptr, &descriptorSetLayout));

      // Descriptor set
      VkDescriptorSetAllocateInfo allocInfo = VulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
      VK_CHECK_RESULT(vkAllocateDescriptorSets(device->vulkanDevice(), &allocInfo, &descriptorSet));
      VkDescriptorImageInfo fontDescriptor = VulkanInitializers::descriptorImageInfo(
         sampler,
         fontView,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      );
      std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
      };
      vkUpdateDescriptorSets(device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
   }

   /** Prepare a separate pipeline for the UI overlay rendering decoupled from the main application */
   void UIOverlay::preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass)
   {
      // Pipeline layout
      // Push constants for UI rendering parameters
      VkPushConstantRange pushConstantRange = VulkanInitializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
      VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VulkanInitializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
      pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
      pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
      VK_CHECK_RESULT(vkCreatePipelineLayout(device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

      // Setup graphics pipeline for UI rendering
      VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
         VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

      VkPipelineRasterizationStateCreateInfo rasterizationState =
         VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

      // Enable blending
      VkPipelineColorBlendAttachmentState blendAttachmentState{};
      blendAttachmentState.blendEnable = VK_TRUE;
      blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
      blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

      VkPipelineColorBlendStateCreateInfo colorBlendState =
         VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

      VkPipelineDepthStencilStateCreateInfo depthStencilState =
         VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

      VkPipelineViewportStateCreateInfo viewportState =
         VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);

      VkPipelineMultisampleStateCreateInfo multisampleState =
         VulkanInitializers::pipelineMultisampleStateCreateInfo(rasterizationSamples);

      std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
      };
      VkPipelineDynamicStateCreateInfo dynamicState =
         VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

      VkGraphicsPipelineCreateInfo pipelineCreateInfo = VulkanInitializers::graphicsPipelineCreateInfo(pipelineLayout, renderPass);

      pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
      pipelineCreateInfo.pRasterizationState = &rasterizationState;
      pipelineCreateInfo.pColorBlendState = &colorBlendState;
      pipelineCreateInfo.pMultisampleState = &multisampleState;
      pipelineCreateInfo.pViewportState = &viewportState;
      pipelineCreateInfo.pDepthStencilState = &depthStencilState;
      pipelineCreateInfo.pDynamicState = &dynamicState;
      pipelineCreateInfo.stageCount = static_cast<uint32_t>(_shaders.size());

      std::vector<VkPipelineShaderStageCreateInfo> shaderInfos;
      for (auto& shader : _shaders)
      {
         shaderInfos.push_back(shader->pipelineShaderStageCreateInfo());
      }
      pipelineCreateInfo.pStages = shaderInfos.data();
      
      pipelineCreateInfo.subpass = subpass;

      // Vertex bindings an attributes based on ImGui vertex definition
      std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      VulkanInitializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
      };
      std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      VulkanInitializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
      VulkanInitializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
      VulkanInitializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
      };
      VkPipelineVertexInputStateCreateInfo vertexInputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();
      vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
      vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
      vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
      vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

      pipelineCreateInfo.pVertexInputState = &vertexInputState;

      VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->vulkanDevice(), pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
   }

   /** Update vertex and index buffer containing the imGui elements when required */
   bool UIOverlay::update()
   {
      ImDrawData* imDrawData = ImGui::GetDrawData();
      bool updateCmdBuffers = false;

      if (!imDrawData) { return false; };

      // Note: Alignment is done inside buffer creation
      VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
      VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

      // Update buffers only if vertex or index count has been changed compared to current buffer size
      if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
         return false;
      }

      // Vertex buffer
      if ((vertexBuffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
         if (vertexBuffer)
         {
            vertexBuffer->unmap();
            delete vertexBuffer;
            vertexBuffer = nullptr;
         }
        
         vertexBuffer = new VulkanBuffer(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBufferSize);
         vertexCount = imDrawData->TotalVtxCount;
         vertexBuffer->map();
         updateCmdBuffers = true;
      }

      // Index buffer
      VkDeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);
      if ((indexBuffer == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
         if (indexBuffer)
         {
            indexBuffer->unmap();
            delete indexBuffer;
            indexBuffer = nullptr;
         }
         indexBuffer = new VulkanBuffer(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBufferSize);
         indexCount = imDrawData->TotalIdxCount;
         indexBuffer->map();
         updateCmdBuffers = true;
      }

      // Upload data
      ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer->_mapped;
      ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer->_mapped;

      for (int n = 0; n < imDrawData->CmdListsCount; n++) {
         const ImDrawList* cmd_list = imDrawData->CmdLists[n];
         memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
         memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
         vtxDst += cmd_list->VtxBuffer.Size;
         idxDst += cmd_list->IdxBuffer.Size;
      }

      // Flush to make writes visible to GPU
      vertexBuffer->flush();
      indexBuffer->flush();

      return updateCmdBuffers;
   }

   void UIOverlay::draw(const VkCommandBuffer commandBuffer)
   {
      ImDrawData* imDrawData = ImGui::GetDrawData();
      int32_t vertexOffset = 0;
      int32_t indexOffset = 0;

      if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
         return;
      }

      ImGuiIO& io = ImGui::GetIO();

      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

      pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
      pushConstBlock.translate = glm::vec2(-1.0f);
      vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

      VkDeviceSize offsets[1] = { 0 };
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->_buffer, offsets);
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer->_buffer, 0, VK_INDEX_TYPE_UINT16);

      for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
      {
         const ImDrawList* cmd_list = imDrawData->CmdLists[i];
         for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
         {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = glm::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = glm::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
         }
         vertexOffset += cmd_list->VtxBuffer.Size;
      }
   }

   void UIOverlay::resize(uint32_t width, uint32_t height)
   {
      ImGuiIO& io = ImGui::GetIO();
      io.DisplaySize = ImVec2((float)(width), (float)(height));
   }

   void UIOverlay::freeResources()
   {
      ImGui::DestroyContext();
      delete vertexBuffer; vertexBuffer = nullptr;
      delete indexBuffer; indexBuffer = nullptr;
      vkDestroyImageView(device->vulkanDevice(), fontView, nullptr);
      vkDestroyImage(device->vulkanDevice(), fontImage, nullptr);
      vkFreeMemory(device->vulkanDevice(), fontMemory, nullptr);
      vkDestroySampler(device->vulkanDevice(), sampler, nullptr);
      vkDestroyDescriptorSetLayout(device->vulkanDevice(), descriptorSetLayout, nullptr);
      vkDestroyDescriptorPool(device->vulkanDevice(), descriptorPool, nullptr);
      vkDestroyPipelineLayout(device->vulkanDevice(), pipelineLayout, nullptr);
      vkDestroyPipeline(device->vulkanDevice(), pipeline, nullptr);
   }

   bool UIOverlay::header(const char* caption)
   {
      return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
   }

   bool UIOverlay::checkBox(const char* caption, bool* value)
   {
      bool res = ImGui::Checkbox(caption, value);
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::checkBox(const char* caption, int32_t* value)
   {
      bool val = (*value == 1);
      bool res = ImGui::Checkbox(caption, &val);
      *value = val;
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::inputFloat(const char* caption, float* value, float step, uint32_t precision)
   {
      bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, precision);
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::sliderFloat(const char* caption, float* value, float min, float max)
   {
      bool res = ImGui::SliderFloat(caption, value, min, max);
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max)
   {
      bool res = ImGui::SliderInt(caption, value, min, max);
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items)
   {
      if (items.empty()) {
         return false;
      }
      std::vector<const char*> charitems;
      charitems.reserve(items.size());
      for (size_t i = 0; i < items.size(); i++) {
         charitems.push_back(items[i].c_str());
      }
      uint32_t itemCount = static_cast<uint32_t>(charitems.size());
      bool res = ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
      if (res) { updated = true; };
      return res;
   }

   bool UIOverlay::button(const char* caption)
   {
      bool res = ImGui::Button(caption);
      if (res) { updated = true; };
      return res;
   }

   void UIOverlay::text(const char* formatstr, ...)
   {
      va_list args;
      va_start(args, formatstr);
      ImGui::TextV(formatstr, args);
      va_end(args);
   }
}