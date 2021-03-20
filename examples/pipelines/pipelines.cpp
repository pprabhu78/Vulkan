/*
 * Vulkan Example - Using different pipelines in one single renderpass
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 /*
  * This sample shows how to create and use multiple pipelines
  * Multiple pipelines with different shaders are created at startup and used during a single draw
  */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample: public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	// This structure is used to pass data to the shaders for the currently bound pipeline
	// It's layout machtes the one from the shaders
	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	// We create and use multiple pipelines in this example
	struct Pipelines {
		VkPipeline phong;
		VkPipeline wireframe;
		VkPipeline toon;
	} pipelines{};

	// All pipelines share the same layout (same descriptors)
	VkPipelineLayout pipelineLayout;

	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Pipeline state objects";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
		camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPerspective(60.0f, (float)(width / 3.0f) / (float)height, 0.1f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.phong, nullptr);
			vkDestroyPipeline(device, pipelines.toon, nullptr);
			if (pipelines.wireframe != VK_NULL_HANDLE) {
				vkDestroyPipeline(device, pipelines.wireframe, nullptr);
			}
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		// Fill mode non solid is required for the wireframe display pipeline, so we can only enable it if it's supported by the device
		if (deviceFeatures.fillModeNonSolid) {
			enabledFeatures.fillModeNonSolid = VK_TRUE;
			// Same for line widths > 1.0f, which are only available of the device supports the wide lines feature
			if (deviceFeatures.wideLines) {
				enabledFeatures.wideLines = VK_TRUE;
			}
		};
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount());
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets - All pipelines use the same descriptors
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Create a pipeline layout used by all pipelines
		// The layout for a pipeline contains the descriptor layout(s) required for that pipeline
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Create three different pipelines
		// Most of the state is shared
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState  = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

		// Create the different graphics pipeline state objects

		// We are using this pipeline as the base for the other pipelines (derivatives)
		// Pipeline derivatives can be used for pipelines that share most of their state
		// Depending on the implementation this may result in better performance for pipeline
		// switching and faster creation time
		pipelineCI.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

		// Phong-shading pipeline
		shaderStages[0] = loadShader(getShadersPath() + "pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.phong));

		// All pipelines created after the base pipeline will be derivatives
		pipelineCI.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		// Base pipeline will be our first created pipeline
		pipelineCI.basePipelineHandle = pipelines.phong;
		// It's only allowed to either use a handle or index for the base pipeline
		// As we use the handle, we must set the index to -1 (see section 9.5 of the specification)
		pipelineCI.basePipelineIndex = -1;

		// Toon shading pipeline
		shaderStages[0] = loadShader(getShadersPath() + "pipelines/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pipelines/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.toon));

		// Pipeline for wire frame rendering
		// We only create this pipeline if the device supports non solid rendering (this is not a mandatory Vulkan feature)
		if (deviceFeatures.fillModeNonSolid) {
			rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
			shaderStages[0] = loadShader(getShadersPath() + "pipelines/wireframe.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getShadersPath() + "pipelines/wireframe.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe));
		}
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame resources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
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

		// Update uniform data for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		// Bind the shared descriptor set for this frame that passes uniform data to the shaders for all pipelines
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		// Bind the buffers for the scene only once
		scene.bindBuffers(commandBuffer);

		// Draw the scene using different pipelines
		
		// Left : Solid colored
		viewport.width = (float)width / 3.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
		scene.draw(commandBuffer);

		// Center : Toon
		viewport.x = (float)width / 3.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toon);
		// Line width > 1.0f only if wide lines feature is supported
		if (deviceFeatures.wideLines) {
			vkCmdSetLineWidth(commandBuffer, 2.0f);
		}
		scene.draw(commandBuffer);

		if (deviceFeatures.fillModeNonSolid) {
			// Right : Wireframe (only if the non solid feature is supported)
			viewport.x = (float)width / 3.0f + (float)width / 3.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
			scene.draw(commandBuffer);
		}

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (!deviceFeatures.fillModeNonSolid) {
			if (overlay->header("Info")) {
				overlay->text("Non solid fill modes not supported!");
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()