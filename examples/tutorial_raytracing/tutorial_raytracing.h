#pragma once

#include "vulkanexamplebase.h"

// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
   uint64_t deviceAddress = 0;
   VkBuffer handle = VK_NULL_HANDLE;
   VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Ray tracing acceleration structure
struct AccelerationStructure {
   VkAccelerationStructureKHR handle;
   uint64_t deviceAddress = 0;
   VkDeviceMemory memory;
   VkBuffer buffer;
};

class TutorialRayTracing : public VulkanExampleBase
{
public:
   TutorialRayTracing();
   virtual ~TutorialRayTracing();

public:
   virtual RayTracingScratchBuffer createScratchBuffer(VkDeviceSize size);
   virtual void deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer);
   virtual void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
   virtual uint64_t getBufferDeviceAddress(VkBuffer buffer);

   virtual void createStorageImage();
   virtual void createBottomLevelAccelerationStructure();
   virtual void createTopLevelAccelerationStructure();
   virtual void createShaderBindingTable();
   virtual void createDescriptorSets();
   virtual void createRayTracingPipeline();
   virtual void createUniformBuffer();
   virtual void handleResize();
   virtual void buildCommandBuffers();
   virtual void updateUniformBuffers();
   virtual void getEnabledFeatures();
   virtual void prepare();
   virtual void draw();
   virtual void render();
public:
   PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
   PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
   PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
   PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
   PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
   PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
   PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
   PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
   PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
   PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

   VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rayTracingPipelineProperties{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

   VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};

   AccelerationStructure bottomLevelAS{};
   AccelerationStructure topLevelAS{};

   vks::Buffer vertexBuffer;
   vks::Buffer indexBuffer;
   uint32_t indexCount;
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

   struct UniformData {
      glm::mat4 viewInverse;
      glm::mat4 projInverse;
   } uniformData;
   vks::Buffer ubo;

   VkPipeline pipeline;
   VkPipelineLayout pipelineLayout;
   VkDescriptorSet descriptorSet;
   VkDescriptorSetLayout descriptorSetLayout;
};
