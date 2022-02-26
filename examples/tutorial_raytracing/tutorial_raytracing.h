#pragma once

#include "../genesis/VulkanExampleBase.h"
#include "GenMath.h"
#include "Camera.h"

#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/tutorial_raytracing/rayTracingInputOutput.h"
#include "../data/shaders/glsl/common/sceneUbo.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class Texture;
   class VulkanGltfModel;
   class Texture;
   class Shader;
   class AccelerationStructure;
   class StorageImage;
   class RenderPass;
   class VulkanBuffer;
   class Tlas;
   class IndirectLayout;
   class CellManager;
   class ShaderBindingTable;
}

class TutorialRayTracing : public genesis::VulkanExampleBase
{
public:
   TutorialRayTracing();
   virtual ~TutorialRayTracing();

public:
   virtual void windowResized() override;
   virtual void keyPressed(uint32_t key) override;
   virtual void render() override;
   virtual void viewChanged() override;
   virtual void enableFeatures() override;
   virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay) override;
   virtual void setupRenderPass() override;
   virtual void prepare() override;
   virtual void buildCommandBuffers() override;
public:
   virtual void draw();

   virtual void createSceneUbo();
   virtual void updateSceneUbo();

   virtual void createScene();

   virtual void saveScreenShot(void);
   virtual void resetCamera(void);

   virtual void rayTrace(int commandBufferIndex);

   virtual void drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

protected:
   virtual void destroyRayTracingStuff();
   virtual void destroyRasterizationStuff();
   virtual void destroyCommonStuff();

   virtual void createAndUpdateDescriptorSets();
   virtual void createAndUpdateRasterizationDescriptorSets();
   virtual void createAndUpdateRayTracingDescriptorSets();

   virtual void createPipelines();
   virtual void createRasterizationPipeline();
   virtual void createRayTracingPipeline();

   virtual void createStorageImages(void);
   virtual void deleteStorageImages(void);
   virtual void writeStorageImageDescriptors(void);

   virtual void buildRasterizationCommandBuffers(void);

public:
   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddressFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR _enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _enabledAccelerationStructureFeatures{};
   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};
   VkPhysicalDeviceShaderClockFeaturesKHR _physicalDeviceShaderClockFeaturesKHR{};


   // Ray tracing
   VkPipeline _rayTracingPipeline;
   VkPipelineLayout _rayTracingPipelineLayout;
   VkDescriptorSet _rayTracingDescriptorSet;
   VkDescriptorSetLayout _rayTracingDescriptorSetLayout;

   VkDescriptorPool _rayTracingDescriptorPool = VK_NULL_HANDLE;

   genesis::ShaderBindingTable* _shaderBindingTable = nullptr;

   genesis::StorageImage* _finalImageToPresent;
   genesis::StorageImage* _intermediateImage;


   // Rasterization
   VkPipelineLayout _rasterizationPipelineLayout;
   VkPipeline _rasterizationPipeline;
   VkPipeline _rasterizationPipelineWireframe;

   VkPipeline _skyBoxRasterizationPipeline;
   VkPipeline _skyBoxRasterizationPipelineWireframe;

   VkDescriptorPool _rasterizationDescriptorPool = VK_NULL_HANDLE;

   VkDescriptorSetLayout _rasterizationDescriptorSetLayout;
   VkDescriptorSet _rasterizationDescriptorSet;
   
   // common
   genesis::Buffer* _sceneUbo;
   PushConstants _pushConstants;

   genesis::Image* _skyCubeMapImage = nullptr;
   genesis::Texture* _skyCubeMapTexture = nullptr;

   genesis::CellManager* _cellManager = nullptr;

   // work around so as to use the same mechanism to render
   genesis::CellManager* _skyBoxManager = nullptr;
   VkPipelineLayout _rasterizationSkyBoxPipelineLayout;

   enum RenderMode
   { 
      RAYTRACE = 0
      , RASTERIZATION = 1 
      , NUM_MODES = 2
   };

   RenderMode _mode = RAYTRACE;

   bool _wireframe = false;
};
