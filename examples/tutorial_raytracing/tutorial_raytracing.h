#pragma once

#include "../genesis/VulkanExampleBase.h"
#include "GenMath.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class VulkanGltfModel;
   class Texture;
   class Shader;
   class AccelerationStructure;
   class StorageImage;
   class RenderPass;
}

// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
   uint64_t deviceAddress = 0;
   VkBuffer handle = VK_NULL_HANDLE;
   VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct PushConstants
{
   genesis::Vector4_32 clearColor;
   int frameIndex;
   float textureLodBias;
   float contributionFromEnvironment;
   float pad;
};

class TutorialRayTracing : public genesis::VulkanExampleBase
{
public:
   TutorialRayTracing();
   virtual ~TutorialRayTracing();

public:
   virtual void windowResized() override;
   virtual void keyPressed(uint32_t key) override;
public:
   virtual RayTracingScratchBuffer createScratchBuffer(VkDeviceSize size);
   virtual void deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer);
   virtual uint64_t getBufferDeviceAddress(VkBuffer buffer);

   virtual void deleteStorageImages(void);
   virtual void createStorageImages(void);
   virtual void writeStorageImageDescriptors(void);
   virtual void createBottomLevelAccelerationStructure();
   virtual void createTopLevelAccelerationStructure();
   virtual void createShaderBindingTable();
   virtual void createDescriptorSets();
   virtual void createRayTracingPipeline();
   virtual void createSceneUbo();

   virtual void rayTrace(int commandBufferIndex);
   virtual void updateSceneUbo();
   virtual void getEnabledFeatures();
   virtual void prepare();
   virtual void draw();
   virtual void render();
   virtual void saveScreenShot(void);
   virtual void resetCamera(void);
   virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay) override;
   virtual void setupRenderPass() override;

   virtual void drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

public:
   VkPhysicalDeviceRayTracingPipelinePropertiesKHR  _rayTracingPipelineProperties{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelerationStructureFeatures{};

   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddresFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR _enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _enabledAccelerationStructureFeatures{};

   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};

   VkPhysicalDeviceShaderClockFeaturesKHR _physicalDeviceShaderClockFeaturesKHR{};

   genesis::AccelerationStructure* topLevelAS = nullptr;

   genesis::VulkanGltfModel* _gltfModel;


   vks::Buffer transformBuffer;
   std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
   vks::Buffer _raygenShaderBindingTable;
   vks::Buffer _missShaderBindingTable;
   vks::Buffer _hitShaderBindingTable;

   VkStridedDeviceAddressRegionKHR _raygenShaderSbtEntry{};
   VkStridedDeviceAddressRegionKHR _missShaderSbtEntry{};
   VkStridedDeviceAddressRegionKHR _hitShaderSbtEntry{};
   VkStridedDeviceAddressRegionKHR _callableShaderSbtEntry{};

   genesis::StorageImage* _finalImageToPresent;
   genesis::StorageImage* _intermediateImage;

   genesis::Buffer* _sceneUbo;

   VkPipeline _rayTracingPipeline;
   VkPipelineLayout _rayTracingPipelineLayout;
   VkDescriptorSet _rayTracingDescriptorSet;
   VkDescriptorSetLayout _rayTracingDescriptorSetLayout;

   PushConstants _pushConstants;
};
