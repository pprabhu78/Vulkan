/*
 * Vulkan Example - Cube map array texture loading and displaying
 *
 * Copyright (C) 2020-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to load a cubemap array texture file into GPU memory and how to use it as a backdrop and a reflection source
 * A cubemap array can store multiple cubemaps in one single Vulkan image
 * The texture loading part can be found in the loadCubemapArray function, and the Texture struct contains all Vulkan objects to store/use a cubemap array texture
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <ktx.h>
#include <ktxvulkan.h>

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	// Contains all Vulkan objects that are required to store and use a cubemap array
	struct CubemapArray {
		VkImage image;
		VkSampler sampler;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width, height;
		uint32_t mipLevels;
		uint32_t layerCount;
	} cubemapArray{};

	bool displaySkybox = true;

	struct Meshes {
		vkglTF::Model skybox;
		std::vector<vkglTF::Model> objects;
		int32_t objectIndex = 0;
		std::vector<std::string> names;
	} models;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::mat4 inverseModelview;
		float lodBias = 0.0f;
		// Selects the layer to be sampled from in the fragment shader
		int selectedCubemapLayer = 1;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	struct Pipelines {
		VkPipeline skybox;
		VkPipeline reflect;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Cube map textures";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));
		camera.setRotationSpeed(0.25f);
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.skybox, nullptr);
			vkDestroyPipeline(device, pipelines.reflect, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			destroyCubemapArray(cubemapArray);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		// The implementation must support cube map arrays for this sample tow ork
		if (deviceFeatures.imageCubeArray) {
			enabledFeatures.imageCubeArray = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support cube map arrays!", VK_ERROR_FEATURE_NOT_PRESENT);
		}

		// Enable anisotropic filtering if supported
		enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	};

	// Loads the cubemap array texture from disk and uploads it to the GPU
	// Cubemaps arrays contain multiple cube maps with 6 faces and each face having it's own set of mip levels
	// The code in this sample creates an array of buffer-to-image copy regions for all cube maps in that array
	// A detailed description of a KTX cube map texture array file can be found below
	void loadCubemapArray(std::string filename, VkFormat format)
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

		// Get properties required for using and upload texture data from the ktx texture object
		cubemapArray.width = ktxTexture->baseWidth;
		cubemapArray.height = ktxTexture->baseHeight;
		cubemapArray.mipLevels = ktxTexture->numLevels;
		cubemapArray.layerCount = ktxTexture->numLayers;
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		vks::Buffer sourceData;

		// Create a host-visible source buffer that contains the raw image data
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &sourceData.buffer));

		// Get memory requirements for the source buffer (alignment, memory type bits)
		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(device, sourceData.buffer, &memReqs);
		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &sourceData.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, sourceData.buffer, sourceData.memory, 0));

		// Copy the ktx image data into the source buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, sourceData.memory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, ktxTextureData, ktxTextureSize);
		vkUnmapMemory(device, sourceData.memory);

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = cubemapArray.mipLevels;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { cubemapArray.width, cubemapArray.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6 * cubemapArray.layerCount;
		// This flag is required for cube map images
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &cubemapArray.image));

		// Allocate memory for the cube map array image
		vkGetImageMemoryRequirements(device, cubemapArray.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &cubemapArray.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, cubemapArray.image, cubemapArray.deviceMemory, 0));

		// We now copy the parts that make up the cube map array to our image via a command buffer
		// Cube map arrays in ktx are stored with a layout like this:
		// - Mip Level 0
		// 	- Layer 0 (= Cube map 0)
		// 		- Face +X
		// 		- Face -X
		// 		- Face +Y
		// 		- Face -Y
		// 		- Face +Z
		// 		- Face -Z
		// 	- Layer 1 (= Cube map 1)
		//		- Face +X
		//		...
		// - Mip Level 1
		// 	- Layer 0 (= Cube map 0)
		// 		- Face +X
		// 		...
		// 	- Layer 1 (= Cube map 1)
		// 		- Face +X
		//		...

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Setup buffer copy regions for each face including all of its miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;
		for (uint32_t face = 0; face < 6; face++) {
			for (uint32_t layer = 0; layer < ktxTexture->numLayers; layer++) {
				for (uint32_t level = 0; level < ktxTexture->numLevels; level++) {
					ktx_size_t offset;
					KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, layer, face, &offset);
					assert(ret == KTX_SUCCESS);
					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = level;
					bufferCopyRegion.imageSubresource.baseArrayLayer = layer * 6 + face;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
					bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
					bufferCopyRegion.imageExtent.depth = 1;
					bufferCopyRegion.bufferOffset = offset;
					bufferCopyRegions.push_back(bufferCopyRegion);
				}
			}
		}

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = cubemapArray.mipLevels;
		subresourceRange.layerCount = 6 * cubemapArray.layerCount;

		// Transition target image to accept the writes from our buffer to image copies
		vks::tools::setImageLayout(
			copyCmd, 
			cubemapArray.image, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			subresourceRange);

		// Copy the cube map array buffer parts from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			copyCmd,
			sourceData.buffer,
			cubemapArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Transition image to shader read layout
		vks::tools::setImageLayout(
			copyCmd, 
			cubemapArray.image, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
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
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = static_cast<float>(cubemapArray.mipLevels);
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sampler.maxAnisotropy = 1.0f;
		if (vulkanDevice->features.samplerAnisotropy)
		{
			sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &cubemapArray.sampler));

		// Create the image view for a cube map array
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = 6 * cubemapArray.layerCount;
		view.subresourceRange.levelCount = cubemapArray.mipLevels;
		view.image = cubemapArray.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &cubemapArray.view));

		// Clean up staging resources
		vkFreeMemory(device, sourceData.memory, nullptr);
		vkDestroyBuffer(device, sourceData.buffer, nullptr);
		ktxTexture_Destroy(ktxTexture);
	}

	// Free all Vulkan resources used by a texture object
	void destroyCubemapArray(CubemapArray cubemapArray)
	{
		if (cubemapArray.image != VK_NULL_HANDLE) {
			vkDestroyImageView(device, cubemapArray.view, nullptr);
			vkDestroyImage(device, cubemapArray.image, nullptr);
			vkDestroySampler(device, cubemapArray.sampler, nullptr);
			vkFreeMemory(device, cubemapArray.deviceMemory, nullptr);
		}
	}

	void loadAssets()
	{
		// Load the models
		uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
		models.names = { "Sphere", "Teapot", "Torusknot", "Venus" };
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue, glTFLoadingFlags);
		}
		// Load the cube map array texture
		loadCubemapArray(getAssetPath() + "textures/cubemap_array.ktx", VK_FORMAT_R8G8B8A8_UNORM);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * getFrameCount())
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			// Binding 1 : Fragment shader image sampler for sampling from the cubemap array
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets

		// Image descriptor for the cube map texture
		VkDescriptorImageInfo cubemapDescriptor =
			vks::initializers::descriptorImageInfo(
				cubemapArray.sampler,
				cubemapArray.view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Fragment shader cubemap sampler
				//  Fragment shader: layout (set = 0, binding = 1) uniform samplerCubeArray samplerCubeMapArray;
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cubemapDescriptor)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout
		const VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
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
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });
		// Skybox pipeline - Uses the cubemap array as a background
		shaderStages[0] = loadShader(getShadersPath() + "texturecubemaparray/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturecubemaparray/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));
		// Reflection pipeline - Uses the cubemap array for reflections on the selected object
		shaderStages[0] = loadShader(getShadersPath() + "texturecubemaparray/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturecubemaparray/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.reflect));
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
		uniformData.inverseModelview = glm::inverse(camera.matrices.view);
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

		// Render the Skybox using the cubemap as a background
		if (displaySkybox) {
			// Note: As the skybox needs to stay at the center, the tranlation part of the modelview matrix is cancelled out in the skybox.vert shader
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
			models.skybox.draw(commandBuffer);
		}

		// Render the selected model using the cubemap as a reflector
		// The cube map layer to be drawn is set via the uniform stored in uniformData.cubemapIndex
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.reflect);
		models.objects[models.objectIndex].draw(commandBuffer);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->sliderInt("Cube map", &uniformData.selectedCubemapLayer, 0, cubemapArray.layerCount - 1);
			overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)cubemapArray.mipLevels);
			overlay->comboBox("Object type", &models.objectIndex, models.names);
			overlay->checkBox("Skybox", &displaySkybox);
		}
	}
};

VULKAN_EXAMPLE_MAIN()