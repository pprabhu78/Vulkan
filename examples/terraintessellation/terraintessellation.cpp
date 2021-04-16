/*
 * Vulkan Example - Dynamic terrain tessellation
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 /*
  * This samples shows how to use tessellation shaders to implement a dynamic level-of-detail and culling system for a terrain renderer
  * The sample generates a low-poly terrain patch from a height map file (see createTerrainPatch) which is then tessellated
  * The tessellation control shader (TCS) implements dynamic per-triangle level-of-detail based on a triangle's screen size, so larger triangles get more tessellation
  * This results in the terrain close to the camera getting more detail than terrain in distance
  * The TCS also does frustum culling on the terrain patches, so patches not inside the current view frustum are culled away
  * The tessellation evaluation shader (TES) then displaces the tessellated patches based on the terrain height map
  * Note: Requires a device that supports tessellation shaders
  */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "frustum.hpp"
#include <ktx.h>
#include <ktxvulkan.h>

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool wireframe = false;
	bool tessellation = true;
	float tessellationFactor = 0.75f;

	// Holds the buffers for the terrain's indices and vertices, which are generated at runtime from a heightmap
	struct Terrain {
		struct Vertices {
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertices;
		struct Indices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} indices;
	} terrain;

	struct Textures {
		vks::Texture2D heightMap;
		vks::Texture2D skySphere;
		vks::Texture2DArray terrainArray;
	} textures;

	struct Models {
		vkglTF::Model skysphere;
	} models;

	// Uniform data contains values for the vertex and tessellation stages
	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 lightPos = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
		glm::vec4 frustumPlanes[6];
		float displacementFactor = 32.0f;
		float tessellationFactor;
		glm::vec2 viewportDim;
		float tessellatedEdgeSize = 20.0f;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		struct DescriptorSets {
			VkDescriptorSet terrain;
			VkDescriptorSet skysphere;
		} descriptorSets;
	};
	std::vector<FrameObjects> frameObjects;

	struct Pipelines {
		VkPipeline terrain;
		VkPipeline wireframe = VK_NULL_HANDLE;
		VkPipeline skysphere;
	} pipelines;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout terrain;
		VkDescriptorSetLayout skysphere;
	} descriptorSetLayouts;

	struct PipelineLayouts {
		VkPipelineLayout terrain;
		VkPipelineLayout skysphere;
	} pipelineLayouts;

	// The tessellation control shader does frustum culling and for that we'll be using a class to calculate the frustum planes
	vks::Frustum frustum;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Dynamic terrain tessellation";
		camera.setType(Camera::CameraType::firstperson);
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(-12.0f, 159.0f, 0.0f));
		camera.setTranslation(glm::vec3(18.0f, 22.5f, 57.5f));
		camera.setMovementSpeed(7.5f);
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.terrain, nullptr);
			if (pipelines.wireframe != VK_NULL_HANDLE) {
				vkDestroyPipeline(device, pipelines.wireframe, nullptr);
			}
			vkDestroyPipeline(device, pipelines.skysphere, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.skysphere, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.terrain, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.terrain, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.skysphere, nullptr);
			textures.heightMap.destroy();
			textures.skySphere.destroy();
			textures.terrainArray.destroy();
			vkDestroyBuffer(device, terrain.vertices.buffer, nullptr);
			vkFreeMemory(device, terrain.vertices.memory, nullptr);
			vkDestroyBuffer(device, terrain.indices.buffer, nullptr);
			vkFreeMemory(device, terrain.indices.memory, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	virtual void getEnabledFeatures()
	{
		// Tessellation shader support is required for this example to work
		if (deviceFeatures.tessellationShader) {
			enabledFeatures.tessellationShader = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support tessellation shaders!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		// Fill mode non solid is required for the wireframe display pipeline, if it's not available, that pipeline can't be selected
		enabledFeatures.fillModeNonSolid = deviceFeatures.fillModeNonSolid;
		// Enable anisotropic filtering if supported
		enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	}

	void loadAssets()
	{
		// Skysphere is drawn using a sphere mesh
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.skysphere.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue, glTFLoadingFlags);
		textures.skySphere.loadFromFile(getAssetPath() + "textures/skysphere_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

		// Terrain textures are stored in a texture array with layers corresponding to terrain height
		textures.terrainArray.loadFromFile(getAssetPath() + "textures/terrain_texturearray_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		// Height data is stored in a one-channel texture
		textures.heightMap.loadFromFile(getAssetPath() + "textures/terrain_heightmap_r16.ktx", VK_FORMAT_R16_UNORM, vulkanDevice, queue);

		// The samplers used for the terrain textures differ ones created by default in Texture2d::loadFromFile, so we destroy these and setup custom samplers
		vkDestroySampler(device, textures.heightMap.sampler, nullptr);
		vkDestroySampler(device, textures.terrainArray.sampler, nullptr);

		// The terrain heightmap will be repeated and mirrored 
		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)textures.heightMap.mipLevels;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &textures.heightMap.sampler));
		textures.heightMap.descriptor.sampler = textures.heightMap.sampler;

		// The terrain texture will be repeated
		samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)textures.terrainArray.mipLevels;
		if (deviceFeatures.samplerAnisotropy) {
			samplerInfo.maxAnisotropy = 4.0f;
			samplerInfo.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &textures.terrainArray.sampler));
		textures.terrainArray.descriptor.sampler = textures.terrainArray.sampler;
	}

	// Creates a terrain quad patch for feeding to the tessellation control shader from a height map
	// This function also pre-calcualtes the normals based on the terrain heightmap
	void createTerrainPatch()
	{
		const uint32_t patchSize = 64;

		// The heightmap will be loaded from a KTX file that stores height as 16 bit values
		const std::string filename = getAssetPath() + "textures/terrain_heightmap_r16.ktx";
		ktxResult ktxLoadingResult;
		ktxTexture* ktxTexture;
#if defined(__ANDROID__)
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);
		ktx_uint8_t* textureData = new ktx_uint8_t[size];
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);
		ktxLoadingResult = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		delete[] textureData;
#else
		ktxLoadingResult = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
		if (ktxLoadingResult != KTX_SUCCESS) {
			vks::tools::exitFatal("Could not load heightmap file", -1);
		}

		ktx_size_t ktxSize = ktxTexture_GetImageSize(ktxTexture, 0);
		ktx_uint8_t* ktxImage = ktxTexture_GetData(ktxTexture);
		uint32_t ktxDim = ktxTexture->baseWidth;
		uint16_t* heightdata = new uint16_t[ktxDim * ktxDim];
		memcpy(heightdata, ktxImage, ktxSize);
		uint32_t scale = ktxDim / patchSize;
		ktxTexture_Destroy(ktxTexture);

		const uint32_t vertexCount = patchSize * patchSize;
		// We use the Vertex definition from the glTF model loader, so we can re-use the vertex input state
		vkglTF::Vertex* vertices = new vkglTF::Vertex[vertexCount];

		// Setup vertices based on the heightmap
		for (uint32_t x = 0; x < patchSize; x++) {
			for (uint32_t y = 0; y < patchSize; y++) {
				// We pre-calcualte the normales based on the heightmap
				// For this we get different height samples centered around the current vertex position and use a 2D sobel filter to calculate the normals
				float heights[3][3];
				for (int32_t hx = -1; hx <= 1; hx++) {
					for (int32_t hy = -1; hy <= 1; hy++) {
						glm::ivec2 rpos = glm::ivec2(x + hx, y + hy) * glm::ivec2(scale);
						rpos.x = std::max(0, std::min(rpos.x, (int)ktxDim - 1));
						rpos.y = std::max(0, std::min(rpos.y, (int)ktxDim - 1));
						rpos /= glm::ivec2(scale);
						heights[hx + 1][hy + 1] = *(heightdata + (rpos.x + rpos.y * ktxDim) * scale) / 65535.0f;
					}
				}
				glm::vec3 normal;
				normal.x = heights[0][0] - heights[2][0] + 2.0f * heights[0][1] - 2.0f * heights[2][1] + heights[0][2] - heights[2][2];
				normal.z = heights[0][0] + 2.0f * heights[1][0] + heights[2][0] - heights[0][2] - 2.0f * heights[1][2] - heights[2][2];
				// Calculate missing up component of the normal using the filtered x and y axis
				normal.y = 0.25f * sqrt(1.0f - normal.x * normal.x - normal.z * normal.z);
				// Set the vertex data for the current position
				uint32_t index = (x + y * patchSize);
				vertices[index].pos[0] = x - (float)patchSize / 2.0f;
				vertices[index].pos[1] = 0.0f;
				vertices[index].pos[2] = y - (float)patchSize / 2.0f;
				vertices[index].pos *= 2.0f;
				vertices[index].uv = glm::vec2((float)x / patchSize, (float)y / patchSize);
				vertices[index].normal = glm::normalize(normal * glm::vec3(2.0f, 1.0f, 2.0f));
			}
		}

		delete[] heightdata;

		// Setup indices
		const uint32_t w = (patchSize - 1);
		const uint32_t indexCount = w * w * 4;
		uint32_t *indices = new uint32_t[indexCount];
		for (auto x = 0; x < w; x++) {
			for (auto y = 0; y < w; y++) {
				uint32_t index = (x + y * w) * 4;
				indices[index] = (x + y * patchSize);
				indices[index + 1] = indices[index] + patchSize;
				indices[index + 2] = indices[index + 1] + 1;
				indices[index + 3] = indices[index] + 1;
			}
		}
		terrain.indices.count = indexCount;

		uint32_t vertexBufferSize = vertexCount * sizeof(vkglTF::Vertex);
		uint32_t indexBufferSize = indexCount * sizeof(uint32_t);

		vks::Buffer vertexStaging, indexStaging;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexStaging,
			vertexBufferSize,
			vertices));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&indexStaging,
			indexBufferSize,
			indices));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize,
			&terrain.vertices.buffer,
			&terrain.vertices.memory));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&terrain.indices.buffer,
			&terrain.indices.memory));

		// Copy vertex and index data to device local buffers
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};
		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, terrain.vertices.buffer, 1, &copyRegion);
		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(copyCmd, indexStaging.buffer, terrain.indices.buffer, 1, &copyRegion);
		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		vertexStaging.destroy();
		indexStaging.destroy();

		delete[] vertices;
		delete[] indices;
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 * getFrameCount())
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2 * getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layouts
		VkDescriptorSetLayoutCreateInfo descriptorLayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

		// Terrain
		setLayoutBindings = {
			// Binding 0 : Shared Tessellation shader ubo
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0),
			// Binding 1 : Height map
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// Binding 3 : Terrain texture array layers
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};
		descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.terrain));

		// Skysphere
		setLayoutBindings = {
			// Binding 0 : Vertex shader ubo
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Color map
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.skysphere));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo;
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;

			// Terrain
			allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.terrain, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSets.terrain));
			writeDescriptorSets = {
				// Binding 0 : Shared tessellation shader ubo
				vks::initializers::writeDescriptorSet(frame.descriptorSets.terrain, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Displacement map
				vks::initializers::writeDescriptorSet(frame.descriptorSets.terrain, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.heightMap.descriptor),
				// Binding 2 : Color map (alpha channel)
				vks::initializers::writeDescriptorSet(frame.descriptorSets.terrain, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.terrainArray.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

			// Skysphere
			allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.skysphere, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSets.skysphere));
			writeDescriptorSets = {
				// Binding 0 : Vertex shader ubo
				vks::initializers::writeDescriptorSet(frame.descriptorSets.skysphere, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor),
				// Binding 1 : Fragment shader color map
				vks::initializers::writeDescriptorSet(frame.descriptorSets.skysphere, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.skySphere.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layouts
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.terrain, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.terrain));
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.skysphere, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.skysphere));

		// Pipelines
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;

		// We render the terrain as a grid of quad patches
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0, VK_FALSE);
		VkPipelineTessellationStateCreateInfo tessellationState = vks::initializers::pipelineTessellationStateCreateInfo(4);
		// Terrain tessellation pipeline
		shaderStages[0] = loadShader(getShadersPath() + "terraintessellation/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "terraintessellation/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[2] = loadShader(getShadersPath() + "terraintessellation/terrain.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStages[3] = loadShader(getShadersPath() + "terraintessellation/terrain.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayouts.terrain, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.pTessellationState = &tessellationState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.terrain));

		// Terrain wireframe pipeline
		if (deviceFeatures.fillModeNonSolid) {
			rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe));
		};

		// Skysphere pipeline
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		// Revert to triangle list topology
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Reset tessellation state
		pipelineCI.pTessellationState = nullptr;
		// Don't write to depth buffer
		depthStencilState.depthWriteEnable = VK_FALSE;
		pipelineCI.stageCount = 2;
		pipelineCI.layout = pipelineLayouts.skysphere;
		shaderStages[0] = loadShader(getShadersPath() + "terraintessellation/skysphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "terraintessellation/skysphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skysphere));
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
		createTerrainPatch();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Calcualte the frustum planes for the current camera perspective
		// These are used by the tessellation control shader to do frustum culling for the terrain patches
		frustum.update(uniformData.projection * uniformData.modelview);
		memcpy(uniformData.frustumPlanes, frustum.planes.data(), sizeof(glm::vec4) * 6);

		// Update uniform-buffers for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelview = camera.matrices.view * glm::mat4(1.0f);
		uniformData.viewportDim = glm::vec2((float)width, (float)height);
		uniformData.tessellationFactor = tessellation ? tessellationFactor : 0.0f;
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

		// Draw the skysphere
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skysphere);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.skysphere, 0, 1, &currentFrame.descriptorSets.skysphere, 0, nullptr);
		models.skysphere.draw(commandBuffer);

		// Draw the terrain patch
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.terrain);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.terrain, 0, 1, &currentFrame.descriptorSets.terrain, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &terrain.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, terrain.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, terrain.indices.count, 1, 0, 0, 0);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {

			overlay->checkBox("Tessellation", &tessellation);
			overlay->inputFloat("Factor", &tessellationFactor, 0.05f, 2);
			if (deviceFeatures.fillModeNonSolid) {
				overlay->checkBox("Wireframe", &wireframe);
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()