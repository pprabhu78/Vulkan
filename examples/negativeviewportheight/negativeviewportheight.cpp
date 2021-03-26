/*
 * Vulkan Example - Using negative viewport heights for changing Vulkan's coordinate system
 *
  * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This samples shows how to use Vulkan's negative viewport feature to change the viewport's coordinate system
 * A negative viewport height is often useful if you need to support multiple graphics APIs and want to use the same coordinate space
 * The sample changes the origin of the viewport to the top-left of the screen similar to OpenGL 
 * It also adds several options to visualize how this affects rendering of clock- and counter-clockwise triangles
 * Note: Requires a device that supports the VK_KHR_MAINTENANCE1 extension or Vulkan 1.1
 */

#include "vulkanexamplebase.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	// Settings for this sample
	bool negativeViewport = true;
	int32_t offsety = 0;
	int32_t offsetx = 0;
	int32_t windingOrder = 1;
	int32_t cullMode = (int32_t)VK_CULL_MODE_BACK_BIT;
	int32_t quadType = 0;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout;
	struct DescriptorSets {
		VkDescriptorSet CW;
		VkDescriptorSet CCW;
	} descriptorSets;

	struct Textures {
		vks::Texture2D CW;
		vks::Texture2D CCW;
	} textures;

	struct FrameObjects : public VulkanFrameObjects {};
	std::vector<FrameObjects> frameObjects;

	// Stores the vertex and index buffers for a textured quad used to demonstrate the negativ viewport feature
	struct Quad {
		vks::Buffer verticesYUp;
		vks::Buffer verticesYDown;
		vks::Buffer indicesCCW;
		vks::Buffer indicesCW;
		void destroy() {
			verticesYUp.destroy();
			verticesYDown.destroy();
			indicesCCW.destroy();
			indicesCW.destroy();
		}
	} quad;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Negative Viewport height";
		settings.overlay = true;
		// [POI] VK_KHR_MAINTENANCE1 is required for using negative viewport heights
		// Note: This is core as of Vulkan 1.1. So if you target 1.1 you don't have to explicitly enable this
		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			textures.CW.destroy();
			textures.CCW.destroy();
			quad.destroy();
			for (FrameObjects& frame : frameObjects) {
				destroyBaseFrameObjects(frame);
			}
		}
	}

	void loadAssets()
	{
		textures.CW.loadFromFile(getAssetPath() + "textures/texture_orientation_cw_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.CCW.loadFromFile(getAssetPath() + "textures/texture_orientation_ccw_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

		// [POI] Create two quads with different Y orientations

		struct Vertex {
			float pos[3];
			float uv[2];
		};

		const float ar = (float)height / (float)width;

		// OpenGL style (y points upwards)
		std::vector<Vertex> verticesYPos = {
			{ -1.0f * ar,  1.0f, 1.0f, 0.0f, 1.0f },
			{ -1.0f * ar, -1.0f, 1.0f, 0.0f, 0.0f },
			{  1.0f * ar, -1.0f, 1.0f, 1.0f, 0.0f },
			{  1.0f * ar,  1.0f, 1.0f, 1.0f, 1.0f },
		};

		// Vulkan style (y points downwards)
		std::vector<Vertex> verticesYNeg = {
			{ -1.0f * ar, -1.0f, 1.0f, 0.0f, 1.0f },
			{ -1.0f * ar,  1.0f, 1.0f, 0.0f, 0.0f },
			{  1.0f * ar,  1.0f, 1.0f, 1.0f, 0.0f },
			{  1.0f * ar, -1.0f, 1.0f, 1.0f, 1.0f },
		};

		const VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYUp, sizeof(Vertex) * 4, verticesYPos.data()));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYDown,  sizeof(Vertex) * 4, verticesYNeg.data()));

		// [POI] Create two set of indices, one for counter clock wise, and one for clock wise rendering
		std::vector<uint32_t> indices;
		indices = { 2,1,0, 0,3,2 };
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCCW, indices.size() * sizeof(uint32_t), indices.data()));
		indices = { 0,1,2, 2,3,0 };
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCW, indices.size() * sizeof(uint32_t), indices.data()));
	}

	void createDescriptors()
	{
		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(1, &poolSize, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));

		VkDescriptorSetAllocateInfo descriptorSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &descriptorSets.CW));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &descriptorSets.CCW));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.CW, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.CW.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.CCW, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.CCW.descriptor)
		};
		vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
	}

	void createPipelines()
	{
		// Layout
		if (pipelineLayout == VK_NULL_HANDLE) {
			VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));
		}

		// Pipeline
		if (pipeline != VK_NULL_HANDLE) {
			vkQueueWaitIdle(queue);
			vkDestroyPipeline(device, pipeline, nullptr);
		}
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.lineWidth = 1.0f;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE + cullMode;
		rasterizationStateCI.frontFace = windingOrder == 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;

		// Vertex bindings and attributes
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX),
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),				// Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3),	// uv
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		VkGraphicsPipelineCreateInfo pipelineCreateInfoCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfoCI.pVertexInputState = &vertexInputState;
		pipelineCreateInfoCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCreateInfoCI.pRasterizationState = &rasterizationStateCI;
		pipelineCreateInfoCI.pColorBlendState = &colorBlendStateCI;
		pipelineCreateInfoCI.pMultisampleState = &multisampleStateCI;
		pipelineCreateInfoCI.pViewportState = &viewportStateCI;
		pipelineCreateInfoCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCreateInfoCI.pDynamicState = &dynamicStateCI;
		pipelineCreateInfoCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfoCI.pStages = shaderStages.data();
		shaderStages[0] = loadShader(getShadersPath() + "negativeviewportheight/quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "negativeviewportheight/quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfoCI, nullptr, &pipeline));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
		}
		loadAssets();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// [POI] Viewport setup
		VkViewport viewport{};
		if (negativeViewport) {
			viewport.x = (float)offsetx;
			// [POI] When using a negative viewport height, the origin needs to be adjusted
			viewport.y = (float)height - offsety;
			viewport.width = (float)width;
			// [POI] Flip the sign of the viewport's height
			viewport.height = -(float)height;
		} else {
			viewport.x = (float)offsetx;
			viewport.y = (float)offsety;
			viewport.width = (float)width;
			viewport.height = (float)height;
		}
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkDeviceSize offsets[1] = { 0 };

		// Render the quad with clock wise and counter clock wise indices, visibility is determined by pipeline settings
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.CW, 0, nullptr);
		vkCmdBindIndexBuffer(commandBuffer, quad.indicesCW.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, quadType == 0 ? &quad.verticesYDown.buffer : &quad.verticesYUp.buffer, offsets);
		vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.CCW, 0, nullptr);
		vkCmdBindIndexBuffer(commandBuffer, quad.indicesCCW.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Scene")) {
			overlay->text("Quad type");
			overlay->comboBox("##quadtype", &quadType, { "VK (y negative)", "GL (y positive)" });
		}

		if (overlay->header("Viewport")) {
			overlay->checkBox("Negative viewport height", &negativeViewport);
			overlay->sliderInt("offset x", &offsetx, -(int32_t)width, (int32_t)width);
			overlay->sliderInt("offset y", &offsety, -(int32_t)height, (int32_t)height);
		}
		if (overlay->header("Pipeline")) {
			overlay->text("Winding order");
			if (overlay->comboBox("##windingorder", &windingOrder, { "clock wise", "counter clock wise" })) {
				createPipelines();
			}
			overlay->text("Cull mode");
			if (overlay->comboBox("##cullmode", &cullMode, { "none", "front face", "back face" })) {
				createPipelines();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()