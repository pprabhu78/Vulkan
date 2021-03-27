/*
 * Vulkan Example - Viewport array with single pass rendering using geometry shaders
 *
 * Copyright (C) 2017-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to use the the viewport array feature and a geometry shader to render a scene from different point of views in one pass
 * To demonstrate this, the sample implements a stereoscopic view setup with two viewports for the left and right eye
 * A geometry shader is used to simultaneously render into both viewports and applies different matrices depending on the target eye
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	// Uniform data is used to pass the matrices for the stereoscopic views to the geometry shader 
	struct UniformData {
		glm::mat4 projection[2];
		glm::mat4 modelview[2];
		glm::vec4 lightPos = glm::vec4(-2.5f, -3.5f, 0.0f, 1.0f);
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	// Camera and view properties
	float eyeSeparation = 0.08f;
	float fov = 90.0f;
	const float focalLength = 0.5f;
	const float zNear = 0.1f;
	const float zFar = 256.0f;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Viewport arrays";
		camera.setType(Camera::CameraType::firstperson);
		camera.setRotation(glm::vec3(0.0f, 90.0f, 0.0f));
		camera.setTranslation(glm::vec3(7.0f, 3.2f, 0.0f));
		camera.setMovementSpeed(5.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
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
		// Geometry shader support is required for this example
		if (deviceFeatures.geometryShader) {
			enabledFeatures.geometryShader = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support geometry shaders!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		// Multiple viewports must be supported
		if (deviceFeatures.multiViewport) {
			enabledFeatures.multiViewport = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support multi viewports!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
	}

	void loadAssets()
	{
		scene.loadFromFile(getAssetPath() + "models/sampleroom.gltf", vulkanDevice, queue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY);
	}

	void createDescriptors()
	{
		// Pool
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount());
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
		// This sample renders to two viewports, so we need to set the viewport and scissor count for this pipeline
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(2, 2, 0);

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
		pipelineCI.renderPass = renderPass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

		shaderStages[0] = loadShader(getShadersPath() + "viewportarray/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "viewportarray/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// A geometry shader is used to output geometry to multiple viewports in a single pass
		// See the "invocations" decorator of the layout input in the shader:
		//	layout (triangles, invocations = 2) in; 
		shaderStages[2] = loadShader(getShadersPath() + "viewportarray/multiview.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
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

		// Update uniform-buffers for the next frame
		// Calculate two matrcies for the left and right eye for a stereoscopic setup that are passed to the geometry shader
		// See http://paulbourke.net/stereographics/stereorender/
		float aspectRatio = (float)(width * 0.5f) / (float)height;
		float wd2 = zNear * tan(glm::radians(fov / 2.0f));
		float ndfl = zNear / focalLength;
		float left, right;
		float top = wd2;
		float bottom = -wd2;

		// Get the values required to calculate the eye matrices from the camera's current position and rotation
		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(camera.rotation.x)) * sin(glm::radians(camera.rotation.y));
		camFront.y = sin(glm::radians(camera.rotation.x));
		camFront.z = cos(glm::radians(camera.rotation.x)) * cos(glm::radians(camera.rotation.y));
		camFront = glm::normalize(camFront);
		glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::mat4 rotation;
		rotation = glm::rotate(glm::mat4(1.0f), glm::radians(camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		// Calculate the matrices for the left and right eyes based on the camera's current position 
		glm::mat4 translation;
		// Left eye
		left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
		translation = glm::translate(glm::mat4(1.0f), camera.position - camRight * (eyeSeparation / 2.0f));
		uniformData.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uniformData.modelview[0] = rotation * translation;
		// Right eye
		left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		translation = glm::translate(glm::mat4(1.0f), camera.position + camRight * (eyeSeparation / 2.0f));
		uniformData.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uniformData.modelview[1] = rotation * translation;

		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// Setup two viewport for each eye of the stereoscopic setup
		VkViewport viewports[2];
		// Left
		viewports[0] = { 0, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };
		// Right
		viewports[1] = { (float)width / 2.0f, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };
		vkCmdSetViewport(commandBuffer, 0, 2, viewports);
		// Use viewport areas for the scissor
		VkRect2D scissorRects[2];
		scissorRects[0] = vks::initializers::rect2D(width / 2, height, 0, 0);
		scissorRects[1] = vks::initializers::rect2D(width / 2, height, width / 2, 0);
		vkCmdSetScissor(commandBuffer, 0, 2, scissorRects);

		// Draw the scene
		// The geometry shader of the currently bound pipeline will run twice and outputs triangles for the left and right eye in one single pass
		// See multiview.geom
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		scene.draw(commandBuffer);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->sliderFloat("Eye separation", &eyeSeparation, -1.0f, 1.0f);
			overlay->sliderFloat("Field of view", &fov, 45.0f, 120.0f);
		}
	}

};

VULKAN_EXAMPLE_MAIN()