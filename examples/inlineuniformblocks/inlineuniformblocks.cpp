/*
 * Vulkan Example - Using inline uniform blocks
 *
 * Copyright (C) 2018-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to use inline uniform blocks for passing uniform data at descriptor set update
 * So instead of having to back descriptors with a (uniform) buffer, you can pass the uniform data from the host during the descriptor set update
 * The sample displays several spheres with different positions and random colors that are passed to shadres during the descriptor set update
 * See createDescriptors for how this is working
 * Note: Requires a device that supports the VK_EXT_inline_uniform_block extension
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model model;

	// Color and position data for each sphere will be passed to the shader using an inline uniform block for each, so we store them in this array
	struct Sphere {
		struct UniformData {
			glm::vec4 color;
			glm::vec4 position;
		} uniformData;
		VkDescriptorSet descriptorSet;
	};
	std::array<Sphere, 16> spheres;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout scene;
		VkDescriptorSetLayout object;
	} descriptorSetLayouts;

	VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformBlockFeatures{};

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Inline uniform blocks";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.0f));
		camera.setRotation(glm::vec3(0.0, 0.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		camera.setMovementSpeed(4.0f);
		camera.setRotationSpeed(0.25f);
		settings.overlay = true;
		srand((unsigned int)time(0));
		
		// Enable the extension required to use inline uniform blocks
		enabledDeviceExtensions.push_back(VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		// We need to enable the inlineUniform feature using the struct provided by the extension
		inlineUniformBlockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT;
		inlineUniformBlockFeatures.inlineUniformBlock = VK_TRUE;
		deviceCreatepNextChain = &inlineUniformBlockFeatures;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.scene, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.object, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	void loadAssets()
	{
		model.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes(2);
		// Global scene matrices uniform buffer
		poolSizes[0] = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * 100);
		// Inline uniform blocks have their own descriptor type
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
		// For inline uniform blocks, the descriptor count member contains the actual data size of the uniform block
		poolSizes[1].descriptorCount = static_cast<uint32_t>(spheres.size()) * sizeof(Sphere::UniformData);
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, static_cast<uint32_t>(spheres.size()) + 999);

		// Additional inline uniform block binding information needs to be chained into the pool's create info using a new structure
		VkDescriptorPoolInlineUniformBlockCreateInfoEXT descriptorPoolInlineUniformBlockCreateInfo{};
		descriptorPoolInlineUniformBlockCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT;
		// Set the number of bindings of this inline uniform block to the number of spheres
		descriptorPoolInlineUniformBlockCreateInfo.maxInlineUniformBlockBindings = static_cast<uint32_t>(spheres.size());
		// Chain into the pool create info
		descriptorPoolCI.pNext = &descriptorPoolInlineUniformBlockCreateInfo;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));

		// Layouts

		// Uniform buffer for global matrices
		VkDescriptorSetLayoutBinding setLayoutBinding;
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.scene));

		// Inline uniform block for the per-object materials
		setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
		setLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		setLayoutBinding.binding = 0;
		// For inline uniform blocks, the descriptor count member contains the actual data size of the uniform block
		setLayoutBinding.descriptorCount = sizeof(Sphere::UniformData);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.object));

		// Sets

		// Scene
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.scene, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}

		// Spheres
		for (auto &sphere : spheres) {
			VkDescriptorSetAllocateInfo descriptorAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.object, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &sphere.descriptorSet));

			// To pass uniform data for this sphere via an inline uniform block, we need to fill the extension-specific structure and chain it into the descriptor set update
			VkWriteDescriptorSetInlineUniformBlockEXT writeDescriptorSetInlineUniformBlock{};
			writeDescriptorSetInlineUniformBlock.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
			// Uniform data to be set with this inline uniform block
			writeDescriptorSetInlineUniformBlock.pData = &sphere.uniformData;
			writeDescriptorSetInlineUniformBlock.dataSize = sizeof(Sphere::UniformData);

			// Update the descriptor and pass the inline uniform block extended data
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
			writeDescriptorSet.dstSet = sphere.descriptorSet;
			writeDescriptorSet.dstBinding = 0;
			// Descriptor count for an inline uniform block contains data sizes of the block
			writeDescriptorSet.descriptorCount = sizeof(Sphere::UniformData);
			// Chain inline uniform block structure
			writeDescriptorSet.pNext = &writeDescriptorSetInlineUniformBlock;
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout
		std::array<VkDescriptorSetLayout, 2> setLayouts;
		setLayouts[0] = descriptorSetLayouts.scene;
		setLayouts[1] = descriptorSetLayouts.object;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });
		shaderStages[0] = loadShader(getShadersPath() + "inlineuniformblocks/inlineuniformblocks.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "inlineuniformblocks/inlineuniformblocks.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	// Setup random colors and fixed positions for the spheres to displayed in this sample
	void setupSpheres()
	{
		for (size_t i = 0; i < spheres.size(); i++) {
			spheres[i].uniformData.color = glm::vec4((float)rand() / (RAND_MAX), (float)rand() / (RAND_MAX), (float)rand() / (RAND_MAX), 1.0f);
			const float rad = glm::radians((float)i * 360.0f / static_cast<uint32_t>(spheres.size()));
			spheres[i].uniformData.position = glm::vec4(glm::vec3(sin(rad), cos(rad), 0.0f) * 3.5f, 1.0f);
		}
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
		setupSpheres();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform-buffers for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;
		uniformData.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

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

		// Render the spheres using the uniform values provided by the inline uniform blocks
		model.bindBuffers(commandBuffer);
		for (size_t i = 0; i < spheres.size(); i++) {
			// Bind the scene matrics to set 0
			// Bind the per-object inline uniform block to set 1 to be read in the vertex shader:
			//	layout (set = 1, binding = 0) uniform InlineUniformBlock {
			//		vec4 color;
			//		vec4 position;
			//	} inlineUniformBlock;
			std::array<VkDescriptorSet, 2> descriptorSets;
			descriptorSets[0] = currentFrame.descriptorSet;
			descriptorSets[1] = spheres[i].descriptorSet;
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
			model.draw(commandBuffer);
		}

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->button("Randomize materials")) {
			// We need to wait for the command buffers to finish before we can update the descriptor sets used by them
			for (FrameObjects& frame : frameObjects) {
				VK_CHECK_RESULT(vkWaitForFences(device, 1, &frame.renderCompleteFence, VK_TRUE, UINT64_MAX));
			}
			// Assign random materials for every sphere and update the descriptors for the inline uniform blocks
			for (auto& sphere : spheres) {
				sphere.uniformData.color = glm::vec4((float)rand() / (RAND_MAX), (float)rand() / (RAND_MAX), (float)rand() / (RAND_MAX), 1.0f);
				// Updating the inline uniform block descriptors is the same as in createDescriptors
				VkWriteDescriptorSetInlineUniformBlockEXT writeDescriptorSetInlineUniformBlock{};
				writeDescriptorSetInlineUniformBlock.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
				writeDescriptorSetInlineUniformBlock.dataSize = sizeof(Sphere::UniformData);
				writeDescriptorSetInlineUniformBlock.pData = &sphere.uniformData;
				// Update the object's inline uniform block to reflect the changes to the sphere's uniform data
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
				writeDescriptorSet.dstSet = sphere.descriptorSet;
				writeDescriptorSet.dstBinding = 0;
				writeDescriptorSet.descriptorCount = sizeof(Sphere::UniformData);
				writeDescriptorSet.pNext = &writeDescriptorSetInlineUniformBlock;
				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}
		}
	}

};

VULKAN_EXAMPLE_MAIN()