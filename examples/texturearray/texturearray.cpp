/*
 * Vulkan Example - Texture arrays and instanced rendering
 *
 * Copyright (C) 2016-2021 Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to load a texture array file into GPU memory and how to display it
 * The texture loading part can be found in the loadTextureArray function, and the TextureArray struct contains all Vulkan objects to store/use a texture
 * To visualize the different layers, the sample draws one cube per texture array layer using instancing and instanced data to select the layer to draw
 */

#include "vulkanexamplebase.h"
#include <ktx.h>
#include <ktxvulkan.h>

#define ENABLE_VALIDATION false
 
 // Max. number of instances to be drawn, needs to be lower or equal to the number of layers in the texture array
constexpr size_t layerCount = 7;

class VulkanExample : public VulkanExampleBase
{
public:
	// Contains all Vulkan objects that are required to store and use a texture array
	struct TextureArray {
		VkImage image;
		VkSampler sampler;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width, height;
	} textureArray{};

	// Vertex layout for this example
	struct Vertex {
		float pos[3];
		float uv[2];
	};

	// Buffers for a cube that the layers of the texture array are displayed on
	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;

	struct UniformData {
		// Scene matrices
		glm::mat4 projection;
		glm::mat4 view;
		// Instanced data for drawing multiple cubes with different texture layers
		// The vec4 stores position in [xyz] and array layer index to sample from in [w]
		glm::vec4 instances[layerCount];
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Texture arrays";
		settings.overlay = true;
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -7.5f));
		camera.setRotation(glm::vec3(-35.0f, 0.0f, 0.0f));
		camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyImageView(device, textureArray.view, nullptr);
			vkDestroyImage(device, textureArray.image, nullptr);
			vkDestroySampler(device, textureArray.sampler, nullptr);
			vkFreeMemory(device, textureArray.deviceMemory, nullptr);
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			vertexBuffer.destroy();
			indexBuffer.destroy();
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	void loadTextureArray(std::string filename, VkFormat format)
	{
		ktxResult result;
		ktxTexture* ktxTexture;

#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		if (!asset) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		ktx_uint8_t *textureData = new ktx_uint8_t[size];
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);
		result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		delete[] textureData;
#else
		if (!vks::tools::fileExists(filename)) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
		assert(result == KTX_SUCCESS);
		// The texture needs to have at least as many layers as we want to draw instances
		if (ktxTexture->numLayers < layerCount) {
			vks::tools::exitFatal("Texture array layer count lower than required array layer count!", -1);
		}

		// Get properties required for using and upload texture data from the ktx texture object
		textureArray.width = ktxTexture->baseWidth;
		textureArray.height = ktxTexture->baseHeight;
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, ktxTextureData, ktxTextureSize);
		vkUnmapMemory(device, stagingMemory);

		// Setup buffer copy regions for array layers
		std::vector<VkBufferImageCopy> bufferCopyRegions;

		// To keep this simple, we will only load layers and no mip level
		for (uint32_t layer = 0; layer < layerCount; layer++)
		{
			// Calculate offset into staging buffer for the current array layer
			ktx_size_t offset;
			KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, layer, 0, &offset);
			assert(ret == KTX_SUCCESS);
			// Setup a buffer image copy structure for the current array layer
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;
			bufferCopyRegions.push_back(bufferCopyRegion);
		}

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { textureArray.width, textureArray.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.arrayLayers = layerCount;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &textureArray.image));

		vkGetImageMemoryRequirements(device, textureArray.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &textureArray.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textureArray.image, textureArray.deviceMemory, 0));

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = layerCount;

		vks::tools::setImageLayout(
			copyCmd,
			textureArray.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			textureArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Change texture image layout to shader read after all faces have been copied
		textureArray.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
			copyCmd,
			textureArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			textureArray.imageLayout,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 8;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &textureArray.sampler));

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = layerCount;
		view.subresourceRange.levelCount = 1;
		view.image = textureArray.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &textureArray.view));

		// Clean up staging resources
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		ktxTexture_Destroy(ktxTexture);
	}

	void loadAssets()
	{
		loadTextureArray(getAssetPath() + "textures/texturearray_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM);
	}

	// Setup buffers for a uv-mapped cube
	void createCube()
	{
		std::vector<Vertex> vertices = {
			{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f } },
			{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f } },

			{ {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f } },
			{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f } },
			{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f } },
			{ {  0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f } },

			{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f } },
			{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f } },

			{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f } },
			{ { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f } },
			{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f } },

			{ {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f } },
			{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f } },

			{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f } },
			{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f } },
		};
		std::vector<uint32_t> indices = {
			0,1,2, 0,2,3, 4,5,6,  4,6,7, 8,9,10, 8,10,11, 12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
		};

		indexCount = static_cast<uint32_t>(indices.size());

		// Create buffers
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

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, getFrameCount())
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Fragment shader image sampler for sampling from texture array
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets

		// Image descriptor for the texture array
		VkDescriptorImageInfo textureDescriptor =
			vks::initializers::descriptorImageInfo(
				textureArray.sampler,
				textureArray.view,
				textureArray.imageLayout);

		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Fragment shader texture array sampler
				//	Fragment shader: layout (set = 0, binding = 1) uniform sampler2DArray samplerArray;
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textureDescriptor)
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
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
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
		shaderStages[0] = loadShader(getShadersPath() + "texturearray/instancing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturearray/instancing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
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
			// We will draw one cube per texture array layer, and pass position and texture array layer index for that cube via instanced data to the shaders
			// These values don't change and are only set once
			float offset = -1.5f;
			float center = (layerCount * offset) / 2.0f - (offset * 0.5f);
			for (uint32_t i = 0; i < layerCount; i++) {
				// Position in [xyz], array layer index to sample from in [w]
				uniformData.instances[i] = glm::vec4(i * offset - center, 0.0f, 0.0f, (float)i);
			}
		}
		loadAssets();
		createCube();
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
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render multiple cubes for each texture array layer using instancing
		vkCmdDrawIndexed(commandBuffer, indexCount, layerCount, 0, 0, 0);
		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

};

VULKAN_EXAMPLE_MAIN()