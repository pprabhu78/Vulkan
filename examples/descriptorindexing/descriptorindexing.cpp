/*
 * Vulkan Example - Descriptor indexing (VK_EXT_descriptor_indexing)
 *
 * Demonstrates use of descriptor indexing to dynamically index into a variable sized array of samples
 * 
 * Relevant code parts are marked with [POI]
 *
 * Copyright (C) 2021 Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	// We will be dynamically indexing into an array of samplers
	std::vector<vks::Texture2D> textures;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	struct DescriptorSetLayouts {
		VkDescriptorSetLayout dynamic;
		VkDescriptorSetLayout constant;
	} descriptorSetLayouts;
	// The descriptor set for the texture array is static, and not required to be per-frame
	VkDescriptorSet textureArraydescriptorSet;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};

	struct Vertex {
		float pos[3];
		float uv[2];
		int32_t textureIndex;
	};

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Descriptor indexing";
		settings.overlay = true;
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.0f));
		camera.setRotation(glm::vec3(-35.0f, 0.0f, 0.0f));
		camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 256.0f);

		// [POI] Enable required extensions
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

		// [POI] Enable required extension features
		physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

		deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.dynamic, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.constant, nullptr);
			vertexBuffer.destroy();
			indexBuffer.destroy();
			for (auto& texture : textures) {
				texture.destroy();
			}
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Create some random textures
	void createTextures()
	{
		textures.resize(32);
		for (size_t i = 0; i < textures.size(); i++) {
			std::random_device rndDevice;
			std::default_random_engine rndEngine(rndDevice());
			std::uniform_int_distribution<short> rndDist(50, 255);
			const int32_t dim = 3;
			const size_t bufferSize = dim * dim * 4;
			std::vector<uint8_t> texture(bufferSize);
			for (size_t i = 0; i < dim * dim; i++) {
				texture[i * 4]     = rndDist(rndEngine);
				texture[i * 4 + 1] = rndDist(rndEngine);
				texture[i * 4 + 2] = rndDist(rndEngine);
				texture[i * 4 + 3] = 255;
			}
			textures[i].fromBuffer(texture.data(), bufferSize, VK_FORMAT_R8G8B8A8_UNORM, dim, dim, vulkanDevice, queue, VK_FILTER_NEAREST);
		}
	}

	// Creates a vertex buffer with a line of cubes with randomized per-face texture indices
	void createCubes()
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// Generate random per-face texture indices
		std::random_device rndDevice;
		std::default_random_engine rndEngine(rndDevice());
		std::uniform_int_distribution<int32_t> rndDist(0, static_cast<uint32_t>(textures.size()) - 1);

		// Generate cubes with random per-face texture indices
		const uint32_t count = 6;
		for (uint32_t i = 0; i < count; i++) {
			// [POI] Get random per-Face texture indices that the shader will sample from
			int32_t textureIndices[6];
			for (uint32_t j = 0; j < 6; j++) {
				textureIndices[j] = rndDist(rndEngine);
			}
			// Push vertices to buffer
			float pos = 2.5f * i - (count * 2.5f / 2.0f);
			const std::vector<Vertex> cube = {
				{ { -1.0f + pos, -1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[0] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[0] },
				{ {  1.0f + pos,  1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[0] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[0] },

				{ {  1.0f + pos,  1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[1] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[1] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[1] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[1] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[2] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[2] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[2] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[2] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[3] },
				{ { -1.0f + pos, -1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[3] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[3] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[3] },

				{ {  1.0f + pos,  1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[4] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[4] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[4] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[4] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[5] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[5] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[5] },
				{ { -1.0f + pos, -1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[5] },
			};
			for (auto& vertex : cube) {
				vertices.push_back(vertex);
			}
			// Push indices to buffer
			const std::vector<uint32_t> cubeIndices = {
				0,1,2,0,2,3,
				4,5,6,4,6,7,
				8,9,10,8,10,11,
				12,13,14,12,14,15,
				16,17,18,16,18,19,
				20,21,22,20,22,23
			};
			for (auto& index : cubeIndices) {
				indices.push_back(index + static_cast<uint32_t>(vertices.size()));
			}
		}

		indexCount = static_cast<uint32_t>(indices.size());

		// For the sake of simplicity we won't stage the vertex data to the gpu memory
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexBuffer,
			vertices.size() * sizeof(Vertex),
			vertices.data()));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&indexBuffer,
			indices.size() * sizeof(uint32_t),
			indices.data()));
	}

	// [POI] Set up descriptor sets and set layout
	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(textures.size()))
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1 + getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layouts 
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
		// One layout for the per-frame uniform buffers
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.dynamic));

		// [POI] One layout for the texture array
		// The binding contains a texture array that is dynamically non-uniform sampled from
		// In the fragment shader:
		//	outFragColor = texture(textures[nonuniformEXT(inTexIndex)], inUV);
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(textures.size()));
		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
		setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		setLayoutBindingFlags.bindingCount = 1;
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		// [POI] The fragment shader uses an unsized array of samplers, which has to be marked with the appropriate flag
		// In the fragment shader:
		//	layout (set = 0, binding = 1) uniform sampler2D textures[];
		VkDescriptorBindingFlagsEXT descriptorBindingFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
		setLayoutBindingFlags.pBindingFlags = &descriptorBindingFlags;
		descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.constant));

		// Sets
		// Per-Frame uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.dynamic, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// Texture array

		// [POI] Specify descriptor count as an additional allocation parameters for the descriptor set
		uint32_t variableDescriptorCount = static_cast<uint32_t>(textures.size());
		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {};
		variableDescriptorCountAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
		variableDescriptorCountAllocInfo.descriptorSetCount = 1;
		variableDescriptorCountAllocInfo.pDescriptorCounts  = &variableDescriptorCount;
		// Allocate the descriptor set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.constant, 1);
		allocInfo.pNext = &variableDescriptorCountAllocInfo;		
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &textureArraydescriptorSet));

		// Gather image descriptors for the texture array
		std::vector<VkDescriptorImageInfo> textureDescriptors(textures.size());
		for (size_t i = 0; i < textures.size(); i++) {
			textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			textureDescriptors[i].sampler = textures[i].sampler;;
			textureDescriptors[i].imageView = textures[i].view;
		}
		// [POI] Update the descriptor for the texture array
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.descriptorCount = static_cast<uint32_t>(textures.size());
		writeDescriptorSet.pBufferInfo = 0;
		writeDescriptorSet.dstSet = textureArraydescriptorSet;
		writeDescriptorSet.pImageInfo = textureDescriptors.data();
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	void createPipelines()
	{
		// Layout with the descriptor set layouts for per-frame uniform buffers and single texture array
		const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.dynamic, descriptorSetLayouts.constant };
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		// Vertex bindings and attributes
		VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32_SINT, offsetof(Vertex, textureIndex) }
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// Instacing pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getShadersPath() + "descriptorindexing/descriptorindexing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "descriptorindexing/descriptorindexing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

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
		createTextures();
		createCubes();
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
		uniformData.view = camera.matrices.view;
		uniformData.model = glm::mat4(1.0f);
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
		// [POI] Bind the descriptor sets:
		//   set 0: Uniform buffers
		//   set 1: Texture array
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &textureArraydescriptorSet, 0, nullptr);
		// [POI] Draw the scene geometry, textures are dynamically sourced in the fragment shader:
		// [POI] The fragment shader does non-uniform access into our sampler array using non uniform access:
		//   outFragcolor = texture(textures[nonuniformEXT(inTexIndex)], inUV)
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

};

VULKAN_EXAMPLE_MAIN()