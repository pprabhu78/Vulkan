/*
 * Vulkan Example - Order Independent Transparency rendering
 *
 * Copyright 2016-2021 by Sascha Willems - www.saschawillems.de
 * Copyright by Daemyung Jang - dm86.jang@gmail.com
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to do order independent transparency using a linked list
 * This is done using atomic image load and store in the fragment shader
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	const uint32_t nodeCount = 20;

	struct Models {
		vkglTF::Model sphere;
		vkglTF::Model cube;
	} models;

	struct Node {
		glm::vec4 color;
		float depth;
		uint32_t next;
	};

	struct geometrySBO {
		uint32_t count;
		uint32_t maxNodeCount;
	} geometrySBO;

	struct GeometryPass {
		VkRenderPass renderPass;
		VkFramebuffer framebuffer;
		vks::Buffer geometry;
		vks::Texture headIndex;
		vks::Buffer linkedList;
	} geometryPass;

	struct RenderPassUBO {
		glm::mat4 projection;
		glm::mat4 view;
	} renderPassUBO;

	struct ObjectData {
		glm::mat4 model;
		glm::vec4 color;
	};

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;
	// The descriptor set for the geometry buffers is static, and not required to be per-frame
	VkDescriptorSet geometryDescriptorSet;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout uniformbuffers;
		VkDescriptorSetLayout geometry;
	} descriptorSetLayouts;

	struct Pipelines {
		VkPipeline geometry;
		VkPipeline color;
	} pipelines;
	VkPipelineLayout pipelineLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Order independent transparency rendering";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -6.0f));
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setPerspective(60.0f, (float) width / (float) height, 0.1f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.geometry, nullptr);
			vkDestroyPipeline(device, pipelines.color, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.uniformbuffers, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.geometry, nullptr);
			destroyGeometryPass();
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	void getEnabledFeatures() override
	{
		// The sample makes use of atomic store operations in the fragment shader, so we check if the implementation supports this feature
		if (deviceFeatures.fragmentStoresAndAtomics) {
			enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support stores and atomic operations in the fragment stage", VK_ERROR_FEATURE_NOT_PRESENT);
		}
	};

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		models.sphere.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.cube.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, getFrameCount() + 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		// Create a geometry descriptor set layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// headIndexImage
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			// LinkedListSBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// AtomicSBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.geometry));

		// Layout for passing matrices
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.uniformbuffers));

		// Sets
		// Per-frame for dynamic uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.uniformbuffers, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// One set fo the geometry images and buffers
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.geometry, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &geometryDescriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: headIndexImage
			vks::initializers::writeDescriptorSet(geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &geometryPass.headIndex.descriptor),
			// Binding 1: LinkedListSBO
			vks::initializers::writeDescriptorSet(geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &geometryPass.linkedList.descriptor),
			// Binding 2: GeometrySBO
			vks::initializers::writeDescriptorSet(geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &geometryPass.geometry.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void prepareGeometryPass()
	{
		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		// Geometry render pass doesn't need any output attachment.
		VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = 0;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &geometryPass.renderPass));

		// Geometry frame buffer doesn't need any output attachment.
		VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = geometryPass.renderPass;
		fbufCreateInfo.attachmentCount = 0;
		fbufCreateInfo.width = width;
		fbufCreateInfo.height = height;
		fbufCreateInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &geometryPass.framebuffer));

		// Create a buffer for GeometrySBO
		// Using the device memory will be best but I will use the host visible buffer to make this example simple.
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&geometryPass.geometry,
			sizeof(geometrySBO)));

		VK_CHECK_RESULT(geometryPass.geometry.map());

		// Set up GeometrySBO data.
		geometrySBO.count = 0;
		geometrySBO.maxNodeCount = nodeCount * width * height;
		memcpy(geometryPass.geometry.mapped, &geometrySBO, sizeof(geometrySBO));

		// Create a texture for HeadIndex.
		// This image will track the head index of each fragment.
		geometryPass.headIndex.device = vulkanDevice;

		VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R32_UINT;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &geometryPass.headIndex.image));

		geometryPass.headIndex.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, geometryPass.headIndex.image, &memReqs);

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &geometryPass.headIndex.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, geometryPass.headIndex.image, geometryPass.headIndex.deviceMemory, 0));

		VkImageViewCreateInfo imageViewInfo = vks::initializers::imageViewCreateInfo();
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R32_UINT;
		imageViewInfo.flags = 0;
		imageViewInfo.image = geometryPass.headIndex.image;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewInfo, nullptr, &geometryPass.headIndex.view));

		geometryPass.headIndex.width = width;
		geometryPass.headIndex.height = height;
		geometryPass.headIndex.mipLevels = 1;
		geometryPass.headIndex.layerCount = 1;
		geometryPass.headIndex.descriptor.imageView = geometryPass.headIndex.view;
		geometryPass.headIndex.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		geometryPass.headIndex.sampler = VK_NULL_HANDLE;

		// Create a buffer for LinkedListSBO
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&geometryPass.linkedList,
			sizeof(Node) * geometrySBO.maxNodeCount));

		VK_CHECK_RESULT(geometryPass.linkedList.map());

		// Change HeadIndex image's layout from UNDEFINED to GENERAL
		VkCommandBufferAllocateInfo cmdBufAllocInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		VkCommandBuffer cmdBuf;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdBuf));

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &cmdBufInfo));

		VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.image = geometryPass.headIndex.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));

		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(queue));
	}

	void preparePipelines()
	{
		// Layout
		std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.uniformbuffers, descriptorSetLayouts.geometry };
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
		// Static object data passed using push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ObjectData), 0);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(0, nullptr);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Create a geometry pipeline.
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, geometryPass.renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position });

		shaderStages[0] = loadShader(getShadersPath() + "oit/geometry.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "oit/geometry.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.geometry));

		// Create a color pipeline.
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = &vertexInputInfo;

		shaderStages[0] = loadShader(getShadersPath() + "oit/color.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "oit/color.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.color));
	}

	void destroyGeometryPass()
	{
		vkDestroyRenderPass(device, geometryPass.renderPass, nullptr);
		vkDestroyFramebuffer(device, geometryPass.framebuffer, nullptr);
		geometryPass.geometry.destroy();
		geometryPass.headIndex.destroy();
		geometryPass.linkedList.destroy();
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(renderPassUBO)));
		}
		loadAssets();
		prepareGeometryPass();
		createDescriptors();
		preparePipelines();
		prepared = true;
	}

	void render() override
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Clear previous geometry pass data
		memset(geometryPass.geometry.mapped, 0, sizeof(uint32_t));

		// Update uniform-buffers for the next frame
		renderPassUBO.projection = camera.matrices.perspective;
		renderPassUBO.view = camera.matrices.view;
		memcpy(currentFrame.uniformBuffer.mapped, &renderPassUBO, sizeof(renderPassUBO));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		//vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// Bind uniform buffer to set 0
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		// Bind geometry buffers to set 1
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &geometryDescriptorSet, 0, nullptr);

		VkClearColorValue clearColor;
		clearColor.uint32[0] = 0xffffffff;

		VkImageSubresourceRange subresRange = {};

		subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresRange.levelCount = 1;
		subresRange.layerCount = 1;

		vkCmdClearColorImage(commandBuffer, geometryPass.headIndex.image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresRange);

		// Begin the geometry render pass
		renderPassBeginInfo.renderPass = geometryPass.renderPass;
		renderPassBeginInfo.framebuffer = geometryPass.framebuffer;
		renderPassBeginInfo.clearValueCount = 0;
		renderPassBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry);
		uint32_t dynamicOffset = 0;
		models.sphere.bindBuffers(commandBuffer);

		// Render the scene
		ObjectData objectData;

		objectData.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
		for (int32_t x = 0; x < 5; x++)
		{
			for (int32_t y = 0; y < 5; y++)
			{
				for (int32_t z = 0; z < 5; z++)
				{
					glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x - 2, y - 2, z - 2));
					glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
					objectData.model = T * S;
					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectData), &objectData);
					models.sphere.draw(commandBuffer);
				}
			}
		}

		objectData.color = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f);
		for (uint32_t x = 0; x < 2; x++)
		{
			glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f * x - 1.5f, 0.0f, 0.0f));
			glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
			objectData.model = T * S;
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectData), &objectData);
			models.cube.draw(commandBuffer);
		}

		vkCmdEndRenderPass(commandBuffer);

		// Make a pipeline barrier to guarantee the geometry pass is done
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

		// Begin the color render pass
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = defaultClearValues;

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.color);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	void windowResized() override
	{
		// Since the geometry pass is based on the current resolution, we need to recreate it when the window gets resized
		destroyGeometryPass();
		prepareGeometryPass();
		// We also need to update the descriptors as the image handles have changed
		vkResetDescriptorPool(device, descriptorPool, 0);
		createDescriptors();
		resized = false;
	}
};

VULKAN_EXAMPLE_MAIN()