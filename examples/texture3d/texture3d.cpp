/*
 * Vulkan Example - 3D texture loading (and generation using perlin noise) example
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to load randomly generated data into a 3D texture stored inGPU memory and how to render it on a quad
 * The texture loading part can be found in the @todo
 */

#include "vulkanexamplebase.h"

#define ENABLE_VALIDATION false

// Translation of Ken Perlin's noise generation JAVA implementation (http://mrl.nyu.edu/~perlin/noise/)
class PerlinNoise
{
private:
	uint32_t permutations[512];
	float fade(float t)
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}
	float lerp(float t, float a, float b)
	{
		return a + t * (b - a);
	}
	float grad(int hash, float x, float y, float z)
	{
		// Convert LO 4 bits of hash code into 12 gradient directions
		int h = hash & 15;
		float u = h < 8 ? x : y;
		float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}
public:
	PerlinNoise()
	{
		// Generate random lookup for permutations containing all numbers from 0..255
		std::vector<uint8_t> plookup;
		plookup.resize(256);
		std::iota(plookup.begin(), plookup.end(), 0);
		std::default_random_engine rndEngine(std::random_device{}());
		std::shuffle(plookup.begin(), plookup.end(), rndEngine);
		for (uint32_t i = 0; i < 256; i++)
		{
			permutations[i] = permutations[256 + i] = plookup[i];
		}
	}
	float noise(float x, float y, float z)
	{
		// Find unit cube that contains point
		int32_t X = (int32_t)floor(x) & 255;
		int32_t Y = (int32_t)floor(y) & 255;
		int32_t Z = (int32_t)floor(z) & 255;
		// Find relative x,y,z of point in cube
		x -= floor(x);
		y -= floor(y);
		z -= floor(z);
		// Compute fade curves for each of x,y,z
		float u = fade(x);
		float v = fade(y);
		float w = fade(z);
		// Hash coordinates of the 8 cube corners
		uint32_t A = permutations[X] + Y;
		uint32_t AA = permutations[A] + Z;
		uint32_t AB = permutations[A + 1] + Z;
		uint32_t B = permutations[X + 1] + Y;
		uint32_t BA = permutations[B] + Z;
		uint32_t BB = permutations[B + 1] + Z;
		// And add blended results for 8 corners of the cube;
		float res = 
			lerp(w, lerp(v,
			lerp(u, grad(permutations[AA], x, y, z), grad(permutations[BA], x - 1, y, z)), lerp(u, grad(permutations[AB], x, y - 1, z), grad(permutations[BB], x - 1, y - 1, z))),
			lerp(v, lerp(u, grad(permutations[AA + 1], x, y, z - 1), grad(permutations[BA + 1], x - 1, y, z - 1)), lerp(u, grad(permutations[AB + 1], x, y - 1, z - 1), grad(permutations[BB + 1], x - 1, y - 1, z - 1))));
		return res;
	}
};

// Fractal noise generator based on perlin noise above
class FractalNoise
{
private:
	PerlinNoise perlinNoise;
	uint32_t octaves = 6;
	float persistence = 0.5f;
public:
	float noise(float x, float y, float z)
	{
		float sum = 0.0f;
		float max = 0.0f;
		float frequency = 1.0f;
		float amplitude = 1.0f;
		for (uint32_t i = 0; i < octaves; i++)
		{
			sum += perlinNoise.noise(x * frequency, y * frequency, z * frequency) * amplitude;
			max += amplitude;
			amplitude *= persistence;
			frequency *= 2.0f;
		}
		sum = sum / max;
		return (sum + 1.0f) / 2.0f;
	}
};

class VulkanExample : public VulkanExampleBase
{
public:
	// Contains all Vulkan objects that are required to store and use a 3D texture
	struct Texture {
		VkImage image;
		VkSampler sampler;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		VkDescriptorImageInfo descriptor;
		VkFormat format;
		uint32_t width, height, depth;
		uint32_t mipLevels;
	} texture{};

	// Vertex layout for this example
	struct Vertex {
		float pos[3];
		float uv[2];
		float normal[3];
	};

	// Buffers for the quad to render the texture on
	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 viewPos;
		float depth = 0.0f;
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
		title = "3D textures";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
		camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		settings.overlay = true;
		srand((unsigned int)time(NULL));
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			destroyTextureImage(texture);
			vertexBuffer.destroy();
			indexBuffer.destroy();
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Create all Vulkan resources for the 3D texture (including descriptors)
	void create3DTexture(uint32_t width, uint32_t height, uint32_t depth)
	{
		// A 3D texture is described as width x height x depth
		texture.width = width;
		texture.height = height;
		texture.depth = depth;
		texture.mipLevels = 1;
		texture.format = VK_FORMAT_R8_UNORM;

		// Format support check
		// 3D texture support in Vulkan is mandatory (in contrast to OpenGL) so no need to check if it's supported
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, texture.format, &formatProperties);
		// Check if format supports transfer
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
		{
			std::cout << "Error: Device does not support flag TRANSFER_DST for selected texture format!" << std::endl;
			return;
		}
		// Check if GPU supports requested 3D texture dimensions
		uint32_t maxImageDimension3D(vulkanDevice->properties.limits.maxImageDimension3D);
		if (width > maxImageDimension3D || height > maxImageDimension3D || depth > maxImageDimension3D)
		{
			std::cout << "Error: Requested texture dimensions is greater than supported 3D texture dimension!" << std::endl;
			return;
		}

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = texture.format;
		imageCreateInfo.mipLevels = texture.mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.extent.width = texture.width;
		imageCreateInfo.extent.height = texture.height;
		imageCreateInfo.extent.depth = texture.depth;
		// Set initial layout of the image to undefined
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &texture.image));

		// Device local memory to back up image
		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};
		vkGetImageMemoryRequirements(device, texture.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &texture.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, texture.image, texture.deviceMemory, 0));

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.maxAnisotropy = 1.0;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &texture.sampler));

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.image = texture.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_3D;
		view.format = texture.format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &texture.view));

		// Fill image descriptor image info to be used for the descriptor slot write
		texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texture.descriptor.imageView = texture.view;
		texture.descriptor.sampler = texture.sampler;

		updateNoiseTexture();
	}

	// Generate multi-dimensional noise values and upload it to the 3D texture using a staging buffer
	void updateNoiseTexture()
	{
		std::cout << "Generating " << texture.width << " x " << texture.height << " x " << texture.depth << " noise texture..." << std::endl;
		auto tStart = std::chrono::high_resolution_clock::now();
		// Prepare an array containing the noise values 
		const uint32_t textureBufferSize = texture.width * texture.height * texture.depth;
		uint8_t* data = new uint8_t[textureBufferSize];
		// Fill the array with a fractal noise grid
		FractalNoise fractalNoise;
		const float noiseScale = static_cast<float>(rand() % 10) + 4.0f;
#pragma omp parallel for
		for (int32_t z = 0; z < texture.depth; z++)
		{
			for (int32_t y = 0; y < texture.height; y++)
			{
				for (int32_t x = 0; x < texture.width; x++)
				{
					float nx = (float)x / (float)texture.width;
					float ny = (float)y / (float)texture.height;
					float nz = (float)z / (float)texture.depth;
					float n = fractalNoise.noise(nx * noiseScale, ny * noiseScale, nz * noiseScale);
					n = n - floor(n);
					data[x + y * texture.width + z * texture.width * texture.height] = static_cast<uint8_t>(floor(n * 255));
				}
			}
		}
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Done in " << tDiff << "ms" << std::endl;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		// Buffer object
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = textureBufferSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Allocate host visible memory for data upload
		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy the texture data array into the staging buffer
		uint8_t *mapped;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&mapped));
		memcpy(mapped, data, textureBufferSize);
		vkUnmapMemory(device, stagingMemory);

		// Copy the local (staging) buffer to device memory
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// The sub resource range describes the regions of the image we will be transitioned
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		// The target image will be used as destination for the copy, so we must transfer from our initial undefined image layout to the transfer destination layout
		vks::tools::setImageLayout(
			copyCmd,
			texture.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// The buffer copy region contains describes the source area of the copy, which in our case is the whole texture
		VkBufferImageCopy bufferCopyRegion{};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = texture.width;
		bufferCopyRegion.imageExtent.height = texture.height;
		bufferCopyRegion.imageExtent.depth = texture.depth;

		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			texture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion);

		// Change texture image layout to shader read once the copy is done
		texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
			copyCmd,
			texture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture.imageLayout,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		// Clean up staging resources
		delete[] data;
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	// Free all Vulkan resources used a texture object
	void destroyTextureImage(Texture texture)
	{
		if (texture.image != VK_NULL_HANDLE) {
			vkFreeMemory(device, texture.deviceMemory, nullptr);
			vkDestroyImageView(device, texture.view, nullptr);
			vkDestroyImage(device, texture.image, nullptr);
			vkDestroySampler(device, texture.sampler, nullptr);
		}
	}

	// Setup vertices for a single uv-mapped quad made from two triangles
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
			// Binding 1 : Fragment shader image sampler
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Fragment shader texture sampler
				vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texture.descriptor)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
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
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
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
		shaderStages[0] = loadShader(getShadersPath() + "texture3d/texture3d.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texture3d/texture3d.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
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
		createQuad();
		create3DTexture(128, 128, 128);
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
		if (!paused) {
			// Update the depth slice to be displayed in this frame
			uniformData.depth += frameTimer * 0.15f;
			if (uniformData.depth > 1.0f)
				uniformData.depth = uniformData.depth - 1.0f;
		}
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
			if (overlay->button("Generate new texture")) {
				updateNoiseTexture();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
