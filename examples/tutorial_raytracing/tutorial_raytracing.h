#pragma once

#include "vulkanexamplebase.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class VulkanGltfModel;
   class Texture;
   class Shader;
   class AccelerationStructure;
}

// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
   uint64_t deviceAddress = 0;
   VkBuffer handle = VK_NULL_HANDLE;
   VkDeviceMemory memory = VK_NULL_HANDLE;
};

class TutorialRayTracing : public VulkanExampleBase
{
public:
   TutorialRayTracing();
   virtual ~TutorialRayTracing();

public:
   virtual RayTracingScratchBuffer createScratchBuffer(VkDeviceSize size);
   virtual void deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer);
   virtual uint64_t getBufferDeviceAddress(VkBuffer buffer);

   virtual void createStorageImage();
   virtual void createBottomLevelAccelerationStructure();
   virtual void createTopLevelAccelerationStructure();
   virtual void createShaderBindingTable();
   virtual void createDescriptorSets();
   virtual void createRayTracingPipeline();
   virtual void createSceneUbo();
   virtual void handleResize();
   virtual void buildCommandBuffers();
   virtual void updateSceneUbo();
   virtual void getEnabledFeatures();
   virtual void prepare();
   virtual void draw();
   virtual void render();
public:

   VkPhysicalDeviceRayTracingPipelinePropertiesKHR  _rayTracingPipelineProperties{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelerationStructureFeatures{};

   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddresFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR _enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _enabledAccelerationStructureFeatures{};

   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};

   genesis::AccelerationStructure* topLevelAS = nullptr;

   genesis::VulkanGltfModel* _gltfModel;

   genesis::Device* _device;

   vks::Buffer transformBuffer;
   std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
   vks::Buffer raygenShaderBindingTable;
   vks::Buffer missShaderBindingTable;
   vks::Buffer hitShaderBindingTable;

   struct StorageImage {
      VkDeviceMemory memory;
      VkImage image;
      VkImageView view;
      VkFormat format;
   } storageImage;


   genesis::Buffer* _sceneUbo;
   VkPipeline pipeline;
   VkPipelineLayout pipelineLayout;
   VkDescriptorSet descriptorSet;
   VkDescriptorSetLayout descriptorSetLayout;
};
