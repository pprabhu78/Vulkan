/*
* UI overlay class using ImGui
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include "../external/imgui/imgui.h"

#include <vector>
#include <string>

namespace genesis 
{
	class Device;
	class VulkanBuffer;
	class Shader;

	class UIOverlay 
	{
	public:
		UIOverlay();
		~UIOverlay();
	public:
		void preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass, VkFormat colorFormat, VkFormat depthFormat);
		void prepareResources();

		bool update();
		void draw(const VkCommandBuffer commandBuffer);
		void resize(uint32_t width, uint32_t height);

		void freeResources();

		bool header(const char* caption);
		bool checkBox(const char* caption, bool* value);
		bool checkBox(const char* caption, int32_t* value);
		bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
		bool sliderFloat(const char* caption, float* value, float min, float max);
		bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
		bool comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
		bool button(const char* caption);
		void text(const char* formatstr, ...);
	public:
      Device* _device;

      VkSampleCountFlagBits _rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      uint32_t _subpass = 0;

      VulkanBuffer* _vertexBuffer = 0;
      VulkanBuffer* _indexBuffer = 0;
      int32_t _vertexCount = 0;
      int32_t _indexCount = 0;

      std::vector<Shader*> _shaders;

      VkDescriptorPool _descriptorPool;
      VkDescriptorSetLayout _descriptorSetLayout;
      VkDescriptorSet _descriptorSet;
      VkPipelineLayout _pipelineLayout;
      VkPipeline _pipeline;

      VkDeviceMemory fontMemory = VK_NULL_HANDLE;
      VkImage fontImage = VK_NULL_HANDLE;
      VkImageView fontView = VK_NULL_HANDLE;
      VkSampler sampler;

      struct PushConstBlock {
         glm::vec2 scale;
         glm::vec2 translate;
      } pushConstBlock;

      bool _visible = true;
      bool _updated = false;
      float _scale = 1.0f;
	};
}