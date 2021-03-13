/*
 * Vulkan Example - Instanced mesh rendering, uses a separate vertex buffer for instanced data
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION false
#if defined(__ANDROID__)
#define INSTANCE_COUNT 4096
#else
#define INSTANCE_COUNT 8192
#endif

class VulkanExample : public VulkanExampleBase
{
public:

	struct {
		vks::Texture2DArray rocks;
		vks::Texture2D planet;
	} textures;

	struct {
		vkglTF::Model rock;
		vkglTF::Model planet;
	} models;

	// Per-instance data block
	struct InstanceData {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
		uint32_t texIndex;
	};
	// Contains the instanced data
	struct InstanceBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		size_t size = 0;
		VkDescriptorBufferInfo descriptor;
	} instanceBuffer;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 lightPos = glm::vec4(0.0f, -5.0f, 0.0f, 1.0f);
		float locSpeed = 0.0f;
		float globSpeed = 0.0f;
	} uniformData;
	struct FrameObjects : public VulkanFrameObjects
	{
		vks::Buffer ubo;
		struct DescriptorSets {
			VkDescriptorSet rocks;
			VkDescriptorSet planet;
		} descriptorSets;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipelineLayout pipelineLayout;
	struct {
		VkPipeline rocks;
		VkPipeline planet;
		VkPipeline starfield;
	} pipelines;

	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Instanced mesh rendering";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(5.5f, -1.85f, -18.5f));
		camera.setRotation(glm::vec3(-17.2f, -4.7f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.rocks, nullptr);
			vkDestroyPipeline(device, pipelines.planet, nullptr);
			vkDestroyPipeline(device, pipelines.starfield, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			vkDestroyBuffer(device, instanceBuffer.buffer, nullptr);
			vkFreeMemory(device, instanceBuffer.memory, nullptr);
			textures.rocks.destroy();
			textures.planet.destroy();
			for (FrameObjects& frame : frameObjects) {
				frame.ubo.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		// Enable anisotropic filtering if supported
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	};

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.rock.loadFromFile(getAssetPath() + "models/rock01.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.planet.loadFromFile(getAssetPath() + "models/lavaplanet.gltf", vulkanDevice, queue, glTFLoadingFlags);
		textures.planet.loadFromFile(getAssetPath() + "textures/lavaplanet_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.rocks.loadFromFile(getAssetPath() + "textures/texturearray_rocks_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void createDescriptors()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Fragment shader combined sampler
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Example uses one ubo per frame and two different images
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount() * 2),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, getFrameCount() * 2),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2 * getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo descripotrSetAllocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descripotrSetAllocInfo, &frame.descriptorSets.rocks));
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descripotrSetAllocInfo, &frame.descriptorSets.planet));
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Instanced rocks
				vks::initializers::writeDescriptorSet(frame.descriptorSets.rocks, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.ubo.descriptor),					// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSets.rocks, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.rocks.descriptor),	// Binding 1 : Color map
				// Planet
				vks::initializers::writeDescriptorSet(frame.descriptorSets.planet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.ubo.descriptor),				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSets.planet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.planet.descriptor)	// Binding 1 : Color map
			};
			vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
		}
	}

	void createPipelines()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState =vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
		pipelineCI.stageCount = shaderStages.size();
		pipelineCI.pStages = shaderStages.data();

		// This example uses two different input states, one for the instanced part and one for non-instanced rendering
		VkPipelineVertexInputStateCreateInfo inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

		// Vertex input bindings
		// The instancing pipeline uses a vertex input state with two bindings
		bindingDescriptions = {
			// Binding point 0: Mesh vertex layout description at per-vertex rate
			vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			// Binding point 1: Instanced data at per-instance rate
			vks::initializers::vertexInputBindingDescription(INSTANCE_BUFFER_BIND_ID, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE)
		};

		// Vertex attribute bindings
		// Note that the shader declaration for per-vertex and per-instance attributes is the same, the different input rates are only stored in the bindings:
		// instanced.vert:
		//	layout (location = 0) in vec3 inPos;		Per-Vertex
		//	...
		//	layout (location = 4) in vec3 instancePos;	Per-Instance
		attributeDescriptions = {
			// Per-vertex attributes
			// These are advanced for each vertex fetched by the vertex shader
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Location 1: Normal
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),		// Location 2: Texture coordinates
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8),	// Location 3: Color
			// Per-Instance attributes
			// These are fetched for each instance rendered
			vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 4, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 4: Position
			vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 5, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Location 5: Rotation
			vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 6, VK_FORMAT_R32_SFLOAT,sizeof(float) * 6),			// Location 6: Scale
			vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 7, VK_FORMAT_R32_SINT, sizeof(float) * 7),			// Location 7: Texture array layer index
		};
		inputState.pVertexBindingDescriptions = bindingDescriptions.data();
		inputState.pVertexAttributeDescriptions = attributeDescriptions.data();

		pipelineCI.pVertexInputState = &inputState;

		// Instancing pipeline
		shaderStages[0] = loadShader(getShadersPath() + "instancing/instancing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "instancing/instancing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Use all input bindings and attribute descriptions
		inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
		inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.rocks));

		// Planet rendering pipeline
		shaderStages[0] = loadShader(getShadersPath() + "instancing/planet.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "instancing/planet.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Only use the non-instanced input bindings and attribute descriptions
		inputState.vertexBindingDescriptionCount = 1;
		inputState.vertexAttributeDescriptionCount = 4;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.planet));

		// Star field pipeline
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		shaderStages[0] = loadShader(getShadersPath() + "instancing/starfield.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "instancing/starfield.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Vertices are generated in the vertex shader
		inputState.vertexBindingDescriptionCount = 0;
		inputState.vertexAttributeDescriptionCount = 0;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.starfield));
	}

	// Create the rock position buffers for the instanced draw
	void createInstancingBuffers()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(INSTANCE_COUNT);

		std::default_random_engine rndGenerator(benchmark.active ? 0 : (unsigned)time(nullptr));
		std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
		std::uniform_int_distribution<uint32_t> rndTextureIndex(0, textures.rocks.layerCount);

		// Distribute rocks randomly on two different rings
		for (auto i = 0; i < INSTANCE_COUNT / 2; i++) {
			glm::vec2 ring0 { 7.0f, 11.0f };
			glm::vec2 ring1 { 14.0f, 18.0f };

			float rho, theta;

			// Inner ring
			rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
			theta = 2.0 * M_PI * uniformDist(rndGenerator);
			instanceData[i].pos = glm::vec3(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta));
			instanceData[i].rot = glm::vec3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
			instanceData[i].scale = 1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator);
			instanceData[i].texIndex = rndTextureIndex(rndGenerator);
			instanceData[i].scale *= 0.75f;

			// Outer ring
			rho = sqrt((pow(ring1[1], 2.0f) - pow(ring1[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring1[0], 2.0f));
			theta = 2.0 * M_PI * uniformDist(rndGenerator);
			instanceData[i + INSTANCE_COUNT / 2].pos = glm::vec3(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta));
			instanceData[i + INSTANCE_COUNT / 2].rot = glm::vec3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
			instanceData[i + INSTANCE_COUNT / 2].scale = 1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator);
			instanceData[i + INSTANCE_COUNT / 2].texIndex = rndTextureIndex(rndGenerator);
			instanceData[i + INSTANCE_COUNT / 2].scale *= 0.75f;
		}

		instanceBuffer.size = instanceData.size() * sizeof(InstanceData);

		// Instanced data is static so we copy it to device local memory for better performance
		struct {
			VkDeviceMemory memory;
			VkBuffer buffer;
		} stagingBuffer;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			instanceBuffer.size,
			&stagingBuffer.buffer,
			&stagingBuffer.memory,
			instanceData.data()));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			instanceBuffer.size,
			&instanceBuffer.buffer,
			&instanceBuffer.memory));

		// Copy to staging buffer
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = { };
		copyRegion.size = instanceBuffer.size;
		vkCmdCopyBuffer(
			copyCmd,
			stagingBuffer.buffer,
			instanceBuffer.buffer,
			1,
			&copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		instanceBuffer.descriptor.range = instanceBuffer.size;
		instanceBuffer.descriptor.buffer = instanceBuffer.buffer;
		instanceBuffer.descriptor.offset = 0;

		// Destroy staging resources
		vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);
		vkFreeMemory(device, stagingBuffer.memory, nullptr);
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.ubo, sizeof(UniformData)));
		}
		loadAssets();
		createInstancingBuffers();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform-buffers for the next frame
		if (!paused || camera.updated) {
			uniformData.projection = camera.matrices.perspective;
			uniformData.view = camera.matrices.view;
			if (!paused) {
				uniformData.locSpeed += frameTimer * 0.35f;
				uniformData.globSpeed += frameTimer * 0.01f;
			}
			memcpy(currentFrame.ubo.mapped, &uniformData, sizeof(uniformData));
		}

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		const VkDeviceSize offsets[1] = { 0 };
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// Star field
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.planet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.starfield);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);

		// Planet
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.planet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.planet);
		models.planet.draw(commandBuffer);

		// Instanced rocks
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSets.rocks, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.rocks);
		// Binding point 0 : Mesh vertex buffer
		vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &models.rock.vertices.buffer, offsets);
		// Binding point 1 : Instance data buffer
		vkCmdBindVertexBuffers(commandBuffer, INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);
		// Bind index buffer
		vkCmdBindIndexBuffer(commandBuffer, models.rock.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render instances
		vkCmdDrawIndexed(commandBuffer, models.rock.indices.count, INSTANCE_COUNT, 0, 0, 0);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Statistics")) {
			overlay->text("Instances: %d", INSTANCE_COUNT);
		}
	}
};

VULKAN_EXAMPLE_MAIN()