/*
 * Vulkan Example - Using occlusion query for visibility testing
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 /*
  * This sample shows how to use a query pool for fetching occlusion information on rendered objects
  * The first render pass will render all objects and an occluder while fetching the number of samples passed (non-occluded) into an array
  * This information is then used in a second pass to display the number of samples that passed the occlusion test and to toggle the visuals of fully occluded objects
  */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	struct Models {
		vkglTF::Model teapot;
		vkglTF::Model plane;
		vkglTF::Model sphere;
	} models;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::vec4 color = glm::vec4(0.0f);
		glm::vec4 lightPos = glm::vec4(10.0f, -10.0f, 10.0f, 1.0f);
		float visible;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		struct UniformBuffers {
			vks::Buffer occluder;
			vks::Buffer teapot;
			vks::Buffer sphere;
		} uniformBuffers;
		struct DescriptorSets {
			VkDescriptorSet occluder;
			VkDescriptorSet teapot;
			VkDescriptorSet sphere;
		} descriptorSets;
	};
	std::vector<FrameObjects> frameObjects;

	struct Pipelines {
		VkPipeline solid;
		VkPipeline occluder;
		// This is a simplified pipeline used in the non-visual occlusion pass
		VkPipeline simple;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	// Query pool objectthat stores the occlusion queries
	VkQueryPool queryPool;
	// This array stores the number of samples passed per object (teapot and sphere) and is read from the query pool
	uint64_t passedSamples[2] = { 1,1 };

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Occlusion queries";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -7.5f));
		camera.setRotation(glm::vec3(0.0f, -123.75f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.solid, nullptr);
			vkDestroyPipeline(device, pipelines.occluder, nullptr);
			vkDestroyPipeline(device, pipelines.simple, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			vkDestroyQueryPool(device, queryPool, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffers.occluder.destroy();
				frame.uniformBuffers.sphere.destroy();
				frame.uniformBuffers.teapot.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.plane.loadFromFile(getAssetPath() + "models/plane_z.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.teapot.loadFromFile(getAssetPath() + "models/teapot.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.sphere.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount() * 3);
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, getFrameCount() * 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSets.occluder));
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSets.sphere));
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSets.teapot));
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(frame.descriptorSets.occluder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffers.occluder.descriptor),
				vks::initializers::writeDescriptorSet(frame.descriptorSets.sphere, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffers.sphere.descriptor),
				vks::initializers::writeDescriptorSet(frame.descriptorSets.teapot, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffers.teapot.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });;

		// Solid rendering pipeline
		shaderStages[0] = loadShader(getShadersPath() + "occlusionquery/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "occlusionquery/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));

		// Basic pipeline for coloring occluded objects
		shaderStages[0] = loadShader(getShadersPath() + "occlusionquery/simple.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "occlusionquery/simple.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.simple));

		// Visual pipeline for the occluder
		shaderStages[0] = loadShader(getShadersPath() + "occlusionquery/occluder.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "occlusionquery/occluder.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable blending
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.occluder));
	}

	// Create a query pool for storing the occlusion query result we want to display and use as visibility toggles in the shader
	void createQueryPool()
	{
		VkQueryPoolCreateInfo queryPoolInfo{};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
		queryPoolInfo.queryCount = 2;
		VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.occluder, sizeof(UniformData)));
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.sphere, sizeof(UniformData)));
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.teapot, sizeof(UniformData)));
		}
		loadAssets();
		createQueryPool();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// vkGetQueryResults is used to copy the query results of the last frame into host memory
		vkGetQueryPoolResults(
			device,
			queryPool,
			0,
			2,
			sizeof(passedSamples),
			passedSamples,
			sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT);
	
		// Update uniform data for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;

		// Occluder
		uniformData.visible = 1.0f;
		uniformData.model = glm::scale(glm::mat4(1.0f), glm::vec3(6.0f));
		uniformData.color = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f);
		memcpy(currentFrame.uniformBuffers.occluder.mapped, &uniformData, sizeof(uniformData));

		// Teapot
		// The visibility flag is used in the shader to change the object's color if it's fully occluded
		uniformData.visible = (passedSamples[0] > 0) ? 1.0f : 0.0f;
		uniformData.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
		uniformData.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		memcpy(currentFrame.uniformBuffers.teapot.mapped, &uniformData, sizeof(uniformData));

		// Sphere
		// The visibility flag is used in the shader to change the object's color if it's fully occluded
		uniformData.visible = (passedSamples[1] > 0) ? 1.0f : 0.0f;
		uniformData.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
		uniformData.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
		memcpy(currentFrame.uniformBuffers.sphere.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		// The query pool needs to be reset outside of the render pass before starting the query
		vkCmdResetQueryPool(commandBuffer, queryPool, 0, 2);

		// The first render pass generates the occlusion data, rendering all objects and storing the number of passed samples for the objects in the query pool
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.simple);
		// Occluder first
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.occluder, 0, nullptr);
		models.plane.draw(commandBuffer);
		// Teapot
		// Start the query for the first set of passed samples
		vkCmdBeginQuery(commandBuffer, queryPool, 0, VK_FLAGS_NONE);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.teapot, 0, nullptr);
		models.teapot.draw(commandBuffer);
		// End the query for the second set of passed samples
		vkCmdEndQuery(commandBuffer, queryPool, 0);
		// Sphere
		// Start the query for the first set of passed samples
		vkCmdBeginQuery(commandBuffer, queryPool, 1, VK_FLAGS_NONE);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.sphere, 0, nullptr);
		models.sphere.draw(commandBuffer);
		// End the query for the second set of passed samples
		vkCmdEndQuery(commandBuffer, queryPool, 1);
		vkCmdEndRenderPass(commandBuffer);

		// The second render pass discards the visual result of the first one and displays the actual scene 
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
		// Teapot
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.teapot, 0, nullptr);
		models.teapot.draw(commandBuffer);
		// Sphere
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.sphere, 0, nullptr);
		models.sphere.draw(commandBuffer);
		// Occluder
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.occluder);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.occluder, 0, nullptr);
		models.plane.draw(commandBuffer);
		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Occlusion query results")) {
			overlay->text("Teapot: %d samples passed", passedSamples[0]);
			overlay->text("Sphere: %d samples passed", passedSamples[1]);
		}
	}

};

VULKAN_EXAMPLE_MAIN()
