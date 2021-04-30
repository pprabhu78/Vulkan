/*
 * Vulkan Example - glTF skinned animation
 *
 * Copyright (C) 2020-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 /*
  * This sample builds on the gltfloading sample and shows how to render an animated glTF model using vertex skinning
  * It loads the additional glTF structures required for vertex skinning and converts these into Vulkan objects (see VulkanglTFModel::loadSkins)
  * This requires information on the joints of the model's skeleton passed to the shader
  * Joint matrices are passed via shader storage buffer objects, joint indices and weights (see VulkanglTFModel::loadNode) are passed via ertex attributes
  * The skinning itself is done on the GPU in the vertex shader (skinnedmodel.vert) using the above information
  * See the accompanying README.md for a short tutorial on the data structures and functions required for vertex skinning
  */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#	define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#include "vulkanexamplebase.h"
#include <vulkan/vulkan.h>

#define ENABLE_VALIDATION false

// Contains everything required to render an animated glTF model with vertex skinning in Vulkan
// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
class VulkanglTFModel
{
  public:
	vks::VulkanDevice *vulkanDevice;
	VkQueue            copyQueue;

	// Base glTF structures, see gltfscene sample for details
	struct Vertices
	{
		VkBuffer       buffer;
		VkDeviceMemory memory;
	} vertices;

	struct Indices
	{
		int            count;
		VkBuffer       buffer;
		VkDeviceMemory memory;
	} indices;

	struct Node;

	struct Material
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t  baseColorTextureIndex;
	};

	struct Image
	{
		vks::Texture2D  texture;
		VkDescriptorSet descriptorSet;
	};

	struct Texture
	{
		int32_t imageIndex;
	};

	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t  materialIndex;
	};

	struct Mesh
	{
		std::vector<Primitive> primitives;
	};

	struct Node
	{
		Node *              parent;
		uint32_t            index;
		std::vector<Node *> children;
		Mesh                mesh;
		glm::vec3           translation{};
		glm::vec3           scale{1.0f};
		glm::quat           rotation{};
		int32_t             skin = -1;
		glm::mat4           matrix;
		glm::mat4           getLocalMatrix();
	};

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 jointIndices;
		glm::vec4 jointWeights;
	};

	// Skin structure

	struct Skin
	{
		std::string                  name;
		Node *                       skeletonRoot = nullptr;
		std::vector<glm::mat4>       inverseBindMatrices;
		std::vector<Node *>          joints;
		std::vector<vks::Buffer>     ssbo;
		std::vector<VkDescriptorSet> descriptorSet;
	};

	// Animation related structures

	struct AnimationSampler
	{
		std::string            interpolation;
		std::vector<float>     inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	struct AnimationChannel
	{
		std::string path;
		Node *      node;
		uint32_t    samplerIndex;
	};

	struct Animation
	{
		std::string                   name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float                         start       = std::numeric_limits<float>::max();
		float                         end         = std::numeric_limits<float>::min();
		float                         currentTime = 0.0f;
	};

	std::vector<Image>     images;
	std::vector<Texture>   textures;
	std::vector<Material>  materials;
	std::vector<Node *>    nodes;
	std::vector<Skin>      skins;
	std::vector<Animation> animations;

	uint32_t activeAnimation = 0;
	uint32_t frameCount      = 0;

	VulkanglTFModel(vks::VulkanDevice* device, VkQueue copyQueue, uint32_t frameCount);
	~VulkanglTFModel();
	void      loadImages(tinygltf::Model &input);
	void      loadTextures(tinygltf::Model &input);
	void      loadMaterials(tinygltf::Model &input);
	Node *    findNode(Node *parent, uint32_t index);
	Node *    nodeFromIndex(uint32_t index);
	void loadFromFile(const std::string& filename);
	void      loadSkins(tinygltf::Model &input);
	void      loadAnimations(tinygltf::Model &input);
	void      loadNode(const tinygltf::Node &inputNode, const tinygltf::Model &input, VulkanglTFModel::Node *parent, uint32_t nodeIndex, std::vector<uint32_t> &indexBuffer, std::vector<VulkanglTFModel::Vertex> &vertexBuffer);
	glm::mat4 getNodeMatrix(VulkanglTFModel::Node *node);
	void      updateJoints(VulkanglTFModel::Node *node, uint32_t frameIndex);
	void      updateAnimation(float deltaTime, uint32_t frameIndex);
	void      drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node node, uint32_t frameIndex);
	void      draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, uint32_t frameIndex);
};

class VulkanExample : public VulkanExampleBase
{
  public:
	bool wireframe = false;

	VulkanglTFModel* glTFModel;

	struct UniformData
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f);
	} uniformData;
	struct FrameObjects : public VulkanFrameObjects
	{
		vks::Buffer     ubo;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipelineLayout pipelineLayout;
	struct Pipelines
	{
		VkPipeline solid;
		VkPipeline wireframe = VK_NULL_HANDLE;
	} pipelines;

	struct DescriptorSetLayouts
	{
		VkDescriptorSetLayout matrices;
		VkDescriptorSetLayout textures;
		VkDescriptorSetLayout jointMatrices;
	} descriptorSetLayouts;

	VulkanExample();
	~VulkanExample();
	virtual void getEnabledFeatures();
	void         loadAssets();
	void         createDescriptors();
	void         createPipelines();
	void         prepare();
	virtual void render();
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);
};
