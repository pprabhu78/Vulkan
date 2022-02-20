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
public:

   virtual void deleteStorageImages(void);
   virtual void createStorageImages(void);
   virtual void writeStorageImageDescriptors(void);
   virtual void createScene();
   virtual void createDescriptorSets();
   virtual void createRayTracingPipeline();
   virtual void createSceneUbo();

   virtual void rayTrace(int commandBufferIndex);
   virtual void updateSceneUbo();
   virtual void enableFeatures() override;
   virtual void prepare();
   virtual void draw();
   virtual void render() override;
   virtual void viewChanged() override;
   virtual void saveScreenShot(void);
   virtual void resetCamera(void);
   virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay) override;
   virtual void setupRenderPass() override;

   virtual void drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

public:

   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddressFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR _enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _enabledAccelerationStructureFeatures{};

   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};

   VkPhysicalDeviceShaderClockFeaturesKHR _physicalDeviceShaderClockFeaturesKHR{};

   genesis::Image* _skyCubeMapImage = nullptr;
   genesis::Texture* _skyCubeMapTexture = nullptr;


   genesis::StorageImage* _finalImageToPresent;
   genesis::StorageImage* _intermediateImage;

   genesis::Buffer* _sceneUbo;

   VkPipeline _rayTracingPipeline;
   VkPipelineLayout _rayTracingPipelineLayout;
   VkDescriptorSet _rayTracingDescriptorSet;
   VkDescriptorSetLayout _rayTracingDescriptorSetLayout;

   PushConstants _pushConstants;

   genesis::CellManager* _cellManager = nullptr;

   genesis::ShaderBindingTable* _shaderBindingTable = nullptr;
};
