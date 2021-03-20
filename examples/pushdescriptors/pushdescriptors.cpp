/*
 * Vulkan Example - Push descriptors
 *
 * Copyright (C) 2018-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * Note: Requires a device that supports the VK_KHR_push_descriptor extension
 *
 * This sample shows how tu use push descriptors
 * These apply the concept of push constants to descriptors, meaning that they can be set at command buffer time
 * So instead of updating the descriptors beforehand and just binding them in the command buffer, this allows us to update them from within the command buffer
 * The sample uses this to pass descriptors for per-model textures and matrices to the shader
 */ 

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <glm/gtx/euler_angles.hpp>

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool animate = true;

	// Function pointer and properties for the push descriptor extension
	PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
	VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps{};

	struct Cube {
		glm::vec3 rotation{};
	};
	std::array<Cube, 2> cubes;

	vkglTF::Model model;
	std::array<vks::Texture2D, 2> textures;

	struct CubeUniformData {
		glm::mat4 model;
	};

	struct SceneUniformData {
		glm::mat4 projection;
		glm::mat4 view;
	} sceneUniformData;

	// Dynamic objects need to be duplicated per frame so we can have frames in flight
	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer sceneUniformBuffer;
		std::array<vks::Buffer, 2> cubeUniformBuffers;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Push descriptors";
		settings.overlay = true;
		camera.setType(Camera::CameraType::lookat);
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
		// Enable the extensions required to use push descriptors
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.sceneUniformBuffer.destroy();
				for (size_t i = 0; i < cubes.size(); i++) {
					frame.cubeUniformBuffers[i].destroy();
				}
				destroyBaseFrameObjects(frame);
			}
			for (auto texture : textures) {
				texture.destroy();
			}
		}
	}

	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		};
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		model.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		textures[0].loadFromFile(getAssetPath() + "textures/crate01_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures[1].loadFromFile(getAssetPath() + "textures/crate02_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	// As this sample sets the descriptors at command buffer time, we only need to create a descriptor set layout to define the shader interface
	void createDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		// Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
		descriptorLayoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

	}

	void createPipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()),0);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		VkGraphicsPipelineCreateInfo pipelineCI  = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color});
		shaderStages[0] = loadShader(getShadersPath() + "pushdescriptors/cube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pushdescriptors/cube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame resources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers for the scene
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.sceneUniformBuffer, sizeof(SceneUniformData)));
			// Uniform buffers for the cubes
			for (size_t i = 0; i < cubes.size(); i++) {
				VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.cubeUniformBuffers[i], sizeof(CubeUniformData)));
			}

		}

		// Extension related setup
		// The push descriptor update function is part of an extension so it has to be manually loaded
		vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
		if (!vkCmdPushDescriptorSetKHR) {
			vks::tools::exitFatal("Could not get a valid function pointer for vkCmdPushDescriptorSetKHR", -1);
		}
		// Get device push descriptor properties (to display them)
		PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
		if (!vkGetPhysicalDeviceProperties2KHR) {
			vks::tools::exitFatal("Could not get a valid function pointer for vkGetPhysicalDeviceProperties2KHR", -1);
		}
		// Get the push descriptor properties of the implementation, these are displayed in the user interface
		pushDescriptorProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2KHR physicalDeviceProperties2{};
		physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;		
		// Chain the extension properties into the device properties structure
		physicalDeviceProperties2.pNext = &pushDescriptorProps;
		vkGetPhysicalDeviceProperties2KHR(physicalDevice, &physicalDeviceProperties2);

		loadAssets();
		createDescriptorSetLayout();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform data for the next frame
		if (!paused) {
			// Animate Cubes
			if (animate) {
				cubes[0].rotation.x += 2.5f * frameTimer;
				if (cubes[0].rotation.x > 360.0f) {
					cubes[0].rotation.x -= 360.0f;
				}
				cubes[1].rotation.y += 2.0f * frameTimer;
				if (cubes[1].rotation.x > 360.0f) {
					cubes[1].rotation.x -= 360.0f;
				}
			}
			// Update scene matrices
			sceneUniformData.projection = camera.matrices.perspective;
			sceneUniformData.view = camera.matrices.view;
			memcpy(currentFrame.sceneUniformBuffer.mapped, &sceneUniformData, sizeof(SceneUniformData));
			// Update cube matrices
			for (size_t i = 0; i < cubes.size(); i++) {
				// Rotate and scale the cube matrix
				CubeUniformData cubeUniformData{};
				cubeUniformData.model = glm::translate(glm::mat4(1.0f), (i == 0) ? glm::vec3(-2.0f, 0.0f, 0.0f) : glm::vec3(1.5f, 0.5f, 0.0f));
				cubeUniformData.model = glm::rotate(cubeUniformData.model, glm::radians(cubes[i].rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
				cubeUniformData.model = glm::rotate(cubeUniformData.model, glm::radians(cubes[i].rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
				cubeUniformData.model = glm::rotate(cubeUniformData.model, glm::radians(cubes[i].rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				cubeUniformData.model = glm::scale(cubeUniformData.model, glm::vec3(0.25f));
				memcpy(currentFrame.cubeUniformBuffers[i].mapped, &cubeUniformData, sizeof(CubeUniformData));
			}
		}


		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		model.bindBuffers(commandBuffer);

		// Render two cubes with different descriptor sets using push descriptors
		for (size_t i = 0; i < cubes.size(); i++) {
			// Instead of preparing the descriptor sets up-front, using push descriptors we can set (push) them inside of a command buffer
			// This allows a more dynamic approach without the need to create descriptor sets for each model
			// Note: dstSet for each descriptor set write is left at zero as this is ignored when using push descriptors
			std::array<VkWriteDescriptorSet, 3> writeDescriptorSets{};

			// Scene matrices
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = 0;
			writeDescriptorSets[0].dstBinding = 0;
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[0].pBufferInfo = &currentFrame.sceneUniformBuffer.descriptor;

			// Model matrices
			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = 0;
			writeDescriptorSets[1].dstBinding = 1;
			writeDescriptorSets[1].descriptorCount = 1;
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[1].pBufferInfo = &currentFrame.cubeUniformBuffers[i].descriptor;

			// Model texture
			writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[2].dstSet = 0;
			writeDescriptorSets[2].dstBinding = 2;
			writeDescriptorSets[2].descriptorCount = 1;
			writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[2].pImageInfo = &textures[i].descriptor;

			// Push the descriptor set updates into the current command buffer
			vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 3, writeDescriptorSets.data());

			model.draw(commandBuffer);
		}

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->checkBox("Animate", &animate);
		}
		if (overlay->header("Device properties")) {
			overlay->text("maxPushDescriptors: %d", pushDescriptorProps.maxPushDescriptors);
		}
	}
};

VULKAN_EXAMPLE_MAIN()