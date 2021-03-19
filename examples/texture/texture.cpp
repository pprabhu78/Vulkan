/*
 * Vulkan Example - Texture loading (and display) example (including mip maps)
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to load a 2D texture file into GPU memory and how to render it on a quad
 * The texture loading part can be found in the loadTexture function, and the Texture struct contains all Vulkan objects to store/use a texture
 */

#include "vulkanexamplebase.h"
#include <ktx.h>
#include <ktxvulkan.h>

#define ENABLE_VALIDATION false

// Vertex layout for this example
struct Vertex {
	float pos[3];
	float uv[2];
	float normal[3];
};

class VulkanExample : public VulkanExampleBase
{
public:
	// Contains all Vulkan objects that are required to store and use a texture
	struct Texture {
		VkSampler sampler;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width, height;
		uint32_t mipLevels;
	} texture;

	// Buffers for the quad to render the texture on
	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 viewPos;
		// The LOD bias can be changed from the UI to change how mip layers are selected
		float lodBias = 0.0f;
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
		title = "Texture loading";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
		camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		destroyTextureImage(texture);
		for (FrameObjects& frame : frameObjects) {
			frame.uniformBuffer.destroy();
			destroyBaseFrameObjects(frame);
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		// Enable anisotropic filtering if supported
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		};
	}

	// Upload texture image data to the GPU
	// 
	// Vulkan offers two types of image tiling (memory layout):
	// 
	// Optimal tiled images:
	//	 These are stored in an implementation specific layout matching the capability of the hardware. They usually support more formats and features and are much faster.
	//	 Optimal tiled images are stored on the device and not accessible by the host. So they can't be written directly to (like liner tiled images) and always require
	//	 some sort of data copy, either from a buffer or a linear tiled image.
	//
	// Linear tiled images:
	//	 These are stored as is and can be copied directly to. But due to the linear nature they're not a good match for GPUs and format and feature support is very limited.
	//	 It's not advised to use linear tiled images for anything else than copying from host to GPU if buffer copies are not an option.
	//	 Linear tiling is thus only implemented for learning purposes, one should always prefer optimal tiled image.
	// 
	// In Short: Always use optimal tiled images for rendering
	void loadTexture()
	{
		// We use the Khronos texture format (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
		std::string filename = getAssetPath() + "textures/metalplate01_rgba.ktx";
		// Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

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

		// Get properties required for using and upload texture data from the ktx texture object
		texture.width = ktxTexture->baseWidth;
		texture.height = ktxTexture->baseHeight;
		texture.mipLevels = ktxTexture->numLevels;
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		// We prefer using staging to copy the texture data to a device local optimal image
		VkBool32 useStaging = true;

		// Only use linear tiling if forced
		bool forceLinearTiling = false;
		if (forceLinearTiling) {
			// Don't use linear if format is not supported for (linear) shader sampling
			// Get device properties for the requested texture format
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
			useStaging = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
		}

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};

		if (useStaging) {
			// Copy data to an optimal tiled image
			// This loads the texture data into a host local buffer that is copied to the optimal tiled image on the device

			// Create a host-visible staging buffer that contains the raw image data
			// This buffer will be the data source for copying texture data to the optimal tiled image on the device
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

			// Copy texture data into host local staging buffer
			uint8_t *data;
			VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, ktxTextureData, ktxTextureSize);
			vkUnmapMemory(device, stagingMemory);

			// Setup buffer copy regions for each mip level
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			uint32_t offset = 0;

			for (uint32_t i = 0; i < texture.mipLevels; i++) {
				// Calculate offset into staging buffer for the current mip level
				ktx_size_t offset;
				KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
				assert(ret == KTX_SUCCESS);
				// Setup a buffer image copy structure for the current mip level
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = i;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
				bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;
				bufferCopyRegions.push_back(bufferCopyRegion);
			}

			// Create optimal tiled target image on the device
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.mipLevels = texture.mipLevels;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			// Set initial layout of the image to undefined
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent = { texture.width, texture.height, 1 };
			imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &texture.image));

			vkGetImageMemoryRequirements(device, texture.image, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture.deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device, texture.image, texture.deviceMemory, 0));

			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			// Image memory barriers for the texture image

			// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
			VkImageSubresourceRange subresourceRange = {};
			// Image only contains color data
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// Start at first mip level
			subresourceRange.baseMipLevel = 0;
			// We will transition on all mip levels
			subresourceRange.levelCount = texture.mipLevels;
			// The 2D texture only has one layer
			subresourceRange.layerCount = 1;

			// Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
			VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();;
			imageMemoryBarrier.image = texture.image;
			imageMemoryBarrier.subresourceRange = subresourceRange;
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
			// Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
			// Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			// Copy mip levels from staging buffer
			vkCmdCopyBufferToImage(
				copyCmd,
				stagingBuffer,
				texture.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data());

			// Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
			// Source pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
			// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

			// Clean up staging resources
			vkFreeMemory(device, stagingMemory, nullptr);
			vkDestroyBuffer(device, stagingBuffer, nullptr);
		} else {
			// Copy data to a linear tiled image

			VkImage mappableImage;
			VkDeviceMemory mappableMemory;

			// Load mip map level 0 to linear tiling image
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			imageCreateInfo.extent = { texture.width, texture.height, 1 };
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &mappableImage));

			// Get memory requirements for this image like size and alignment
			vkGetImageMemoryRequirements(device, mappableImage, &memReqs);
			// Set memory allocation size to required memory size
			memAllocInfo.allocationSize = memReqs.size;
			// Get memory type that can be mapped to host memory
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &mappableMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device, mappableImage, mappableMemory, 0));

			// Map image memory
			void *data;
			VK_CHECK_RESULT(vkMapMemory(device, mappableMemory, 0, memReqs.size, 0, &data));
			// Copy image data of the first mip level into memory
			memcpy(data, ktxTextureData, memReqs.size);
			vkUnmapMemory(device, mappableMemory);

			// Linear tiled images don't need to be staged and can be directly used as textures
			texture.image = mappableImage;
			texture.deviceMemory = mappableMemory;

			// Setup image memory barrier transfer image to shader read layout
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			// The sub resource range describes the regions of the image we will be transition
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;

			// Transition the texture image layout to shader read, so it can be sampled from
			VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();;
			imageMemoryBarrier.image = texture.image;
			imageMemoryBarrier.subresourceRange = subresourceRange;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
			// Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
			// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
		}

		ktxTexture_Destroy(ktxTexture);

		// Create a texture sampler
		// In Vulkan textures are accessed by samplers
		// This separates all the sampling information from the texture data. This means you could have multiple sampler objects for the same texture with different settings
		// Note: Similar to the samplers available with OpenGL 3.3
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		// Set max level-of-detail to mip level count of the texture
		sampler.maxLod = (useStaging) ? (float)texture.mipLevels : 0.0f;
		// Enable anisotropic filtering
		// This feature is optional, so we must check if it's supported on the device
		if (vulkanDevice->features.samplerAnisotropy) {
			// Use max. level of anisotropy for this example
			sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		} else {
			// The device does not support anisotropic filtering
			sampler.maxAnisotropy = 1.0;
			sampler.anisotropyEnable = VK_FALSE;
		}
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &texture.sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		// The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
		// It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;
		// Linear tiling usually won't support mip maps
		// Only set mip map count if optimal tiling is used
		view.subresourceRange.levelCount = (useStaging) ? texture.mipLevels : 1;
		// The view will be based on the texture's image
		view.image = texture.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &texture.view));
	}

	// Free all Vulkan resources used by a texture object
	void destroyTextureImage(Texture texture)
	{
		vkDestroyImageView(device, texture.view, nullptr);
		vkDestroyImage(device, texture.image, nullptr);
		vkDestroySampler(device, texture.sampler, nullptr);
		vkFreeMemory(device, texture.deviceMemory, nullptr);
	}

	// Setup buffers for a single uv-mapped quad made from two triangles
	void createQuad()
	{
		std::vector<Vertex> vertices =
		{
			{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		};

		// Setup indices
		std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
		indexCount = static_cast<uint32_t>(indices.size());

		// Create buffers
		// For the sake of simplicity we won't stage the vertex data to the gpu memory
		// Vertex buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexBuffer,
			vertices.size() * sizeof(Vertex),
			vertices.data()));
		// Index buffer
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
			// Binding 1 : Fragment shader combined image sampler for our 2D texture
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));

			// Setup a descriptor image info for the current texture to be used as a combined image sampler
			// We need to setup a descriptor image info for our texture, that'll be used for updating the descriptor slot
			VkDescriptorImageInfo textureDescriptor;
			// The image's view (images are never directly accessed by the shader, but rather through views defining subresources)
			textureDescriptor.imageView = texture.view;
			// The sampler (Telling the shader how to sample the texture, including repeat, border, etc.)
			textureDescriptor.sampler = texture.sampler;
			// The current layout of the image, in our case shader read
			// Note that the image needs to be in that layout, e.g. by doing a manual layout transition
			textureDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;;

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Fragment shader texture sampler
				//	Fragment shader: layout (set = 0, binding = 1) uniform sampler2D samplerColor;
				vks::initializers::writeDescriptorSet(
					frame.descriptorSet,
					// The shader uses a combined image sampler (samplre and image in one descriptor)
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		
					// The combined image sampler is bound to binding point 1
					1,					
					// Point to the image descriptor for our 2D texture
					&textureDescriptor)
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
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		// Vertex input state
		VkVertexInputBindingDescription inputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
		// Attribute descriptions
		std::vector<VkVertexInputAttributeDescription> inputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal))
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &inputBinding;
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(inputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = inputAttributes.data();
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputState;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		shaderStages[0] = loadShader(getShadersPath() + "texture/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texture/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
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
		}
		loadTexture();
		createQuad();
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
		uniformData.viewPos = camera.viewPos;
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
		// Render the textured quad
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)texture.mipLevels);
		}
	}
};

VULKAN_EXAMPLE_MAIN()
