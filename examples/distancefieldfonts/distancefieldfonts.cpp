/*
* Vulkan Example - Font rendering using multi-channel signed distance fields
*
* Font generated using https://github.com/Chlumsky/msdfgen
*
* Copyright (C) 2016~2020 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <assert.h>
#include <vector>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanBuffer.hpp"
#include "json.hpp"

#define ENABLE_VALIDATION false

// Vertex layout for this example
struct Vertex {
	float pos[3];
	float uv[2];
};

struct FontChar {
	float x, y;
	float width;
	float height;
	float xoffset;
	float yoffset;
	float xadvance;
	uint32_t page;
};

// Quick and dirty : complete ASCII table
// Only chars present in the .fnt are filled with data!
std::array<FontChar, 255> fontChars;

class VulkanExample : public VulkanExampleBase
{
public:
	bool useIndexBuffer = false;

	vks::Texture2D fontTexture;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t vertexCount;
	uint32_t indexCount;

	struct {
		vks::Buffer vs;
		vks::Buffer fs;
	} uniformBuffers;

	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 model;
	} uboVS;

	struct UBOFS {
		glm::vec4 outlineColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		float outlineWidth = 0.6f;
		float outline = true;
	} uboFS;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Distance field font rendering";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
		settings.overlay = true;
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		// Clean up texture resources
		fontTexture.destroy();

		vkDestroyPipeline(device, pipeline, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vertexBuffer.destroy();
		indexBuffer.destroy();

		uniformBuffers.vs.destroy();
		uniformBuffers.fs.destroy();
	}

	// Parse font description from .json file
	void parseFontDescription(std::string filename)
	{
		std::ifstream is(filename);
		if (!is.is_open())
		{
			std::cerr << "Error: Could not open font definition file \"" << filename << "\"" << std::endl;
			return;
		}
		nlohmann::json json;
		is >> json;
		is.close();
		for (auto& charinfo : json["chars"]) {
			FontChar& fontchar = fontChars[charinfo["id"]];
			fontchar.x = charinfo["x"];
			fontchar.y = charinfo["y"];
			fontchar.width = charinfo["width"];
			fontchar.height = charinfo["height"];
			fontchar.xoffset = charinfo["xoffset"];
			fontchar.yoffset = charinfo["yoffset"];
			fontchar.xadvance = charinfo["xadvance"];
			fontchar.page = charinfo["page"]; //@todo not used 
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.4f, 0.4f, 0.4f, 1.0f } };;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			// Render Text
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offsets);
			if (useIndexBuffer) {
				vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0);
			}
			else {
				vkCmdDraw(drawCmdBuffers[i], vertexCount, 1, 0, 0);
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// Creates a vertex buffer containing quads for the passed text
	void generateText(std:: string text)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		uint32_t indexOffset = 0;

		float w = fontTexture.width;
		float h = fontTexture.height;

		float posx = 0.0f;
		float posy = 0.0f;

		for (uint32_t i = 0; i < text.size(); i++)
		{
			if (text[i] == '\n') {
				posx = 0.0f;
				posy += 1.0f * 36.0f;
				continue;
			}

			const float factor = 36.0f / 36.0f;

			FontChar *charInfo = &fontChars[(int)text[i]];

			float bw = (charInfo->x + charInfo->width);
			float bh = (charInfo->y + charInfo->height);

			float u0 = charInfo->x / (float)fontTexture.width;
			float v1 = charInfo->y / (float)fontTexture.height;
			float u1 = bw / (float)fontTexture.width;
			float v0 = bh / (float)fontTexture.height;

			float x = posx + charInfo->xoffset;
			float y = posy + charInfo->yoffset;
			float w = charInfo->width;
			float h = charInfo->height;

			if (useIndexBuffer) {
				vertices.push_back({ { x,		y,		0.0f }, { u0, v1 } });
				vertices.push_back({ { x,		y + h,	0.0f }, { u0, v0 } });
				vertices.push_back({ { x + w,	y + h,	0.0f }, { u1, v0 } });
				vertices.push_back({ { x + w,	y,		0.0f }, { u1, v1 } });

				std::array<uint32_t, 6> letterIndices = { 0,1,2, 2,3,0 };
				for (auto& index : letterIndices)
				{
					indices.push_back(indexOffset + index);
				}
				indexOffset += 4;
			}
			else {
				vertices.push_back({ { x,		y,		0.0f }, { u0, v1 } });
				vertices.push_back({ { x,		y + h,	0.0f }, { u0, v0 } });
				vertices.push_back({ { x + w,	y + h,	0.0f }, { u1, v0 } });
				vertices.push_back({ { x + w,	y + h,	0.0f }, { u1, v0 } });
				vertices.push_back({ { x + w,	y,		0.0f }, { u1, v1 } });
				vertices.push_back({ { x,		y,		0.0f }, { u0, v1 } });
			}

			float advance = ((float)(charInfo->xadvance) / factor);
			posx += advance;
		}
		indexCount = indices.size();
		vertexCount = vertices.size();

		// Center
		for (auto& v : vertices)
		{
			v.pos[0] -= posx / 2.0f;
			v.pos[1] -= 0.5f;
		}

		// Generate host accesible buffers for the text vertices and indices and upload the data

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexBuffer,
			vertices.size() * sizeof(Vertex),
			vertices.data()));

		if (useIndexBuffer) {
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&indexBuffer,
				indices.size() * sizeof(uint32_t),
				indices.data()));
		}
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vks::initializers::vertexInputBindingDescription(
				0, 
				sizeof(Vertex), 
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(2);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vks::initializers::vertexInputAttributeDescription(
				0,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);			
		// Location 1 : Texture coordinates
		vertices.attributeDescriptions[1] =
			vks::initializers::vertexInputAttributeDescription(
				0,
				1,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 3);

		vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo = 
			vks::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				2);

		VkResult vkRes = vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);
		assert(!vkRes);
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = 
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				VK_SHADER_STAGE_VERTEX_BIT, 
				0),
			// Binding 1 : Fragment shader image sampler
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				VK_SHADER_STAGE_FRAGMENT_BIT, 
				1),
			// Binding 2 : Fragment shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				2)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout = 
			vks::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		// Image descriptor for the multi-channel signed distance font texture
		VkDescriptorImageInfo texDescriptor =
			vks::initializers::descriptorImageInfo(
				fontTexture.sampler,
				fontTexture.view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
			descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				0, 
				&uniformBuffers.vs.descriptor),
			// Binding 1 : Fragment shader texture sampler
			vks::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				1, 
				&texDescriptor),
			// Binding 2 : Fragment shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				2,
				&uniformBuffers.fs.descriptor)
		};
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_TRUE);

		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(
				1, 
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(
				VK_FALSE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();

		// Multi-channel signed distance field font rendering pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/distancefieldfonts/msdf.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/distancefieldfonts/msdf.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vs,
			sizeof(uboVS)));

		// Fragment sahder uniform buffer block (Contains font rendering parameters)
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.fs,
			sizeof(uboFS)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.vs.map());
		VK_CHECK_RESULT(uniformBuffers.fs.map());

		updateUniformBuffers();
		updateFontSettings();
	}

	void updateUniformBuffers()
	{
		uboVS.projection = camera.matrices.perspective;
		uboVS.model = camera.matrices.view;
		memcpy(uniformBuffers.vs.mapped, &uboVS, sizeof(uboVS));
	}

	void updateFontSettings()
	{
		memcpy(uniformBuffers.fs.mapped, &uboFS, sizeof(uboFS));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		const std::string fontname = "Raleway-Bold";
		parseFontDescription(getAssetPath() + fontname + "-msdf.json");
		fontTexture.loadFromFile(getAssetPath() + fontname + ".ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		generateText("Vulkan");
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			bool outline = (uboFS.outline == 1.0f);
			if (overlay->checkBox("Outline", &outline)) {
				uboFS.outline = outline ? 1.0f : 0.0f;
				updateFontSettings();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()