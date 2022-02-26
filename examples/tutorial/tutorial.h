#pragma once

#include "../genesis/VulkanExampleBase.h"
#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/tutorial/input_output.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class VulkanGltfModel;
   class Texture;
   class Shader;
   class IndirectLayout;
   class CellManager;
}

class Tutorial : public genesis::VulkanExampleBase
{
public:
   Tutorial();
   ~Tutorial();

public:
   virtual void keyPressed(uint32_t key) override;
   virtual void render() override;
   virtual void viewChanged() override;
   virtual void enableFeatures() override;
   virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay) override;
   virtual void buildCommandBuffers() override;
   virtual void prepare() override;

public:
   virtual void draw();

   virtual void createSceneUbo();
   virtual void updateSceneUbo(void);

   virtual void createScene(void);

   virtual void saveScreenShot(void);
   virtual void resetCamera(void);

   virtual void createRasterizationPipeline();
   virtual void createAndUpdateRasterizationDescriptorSets(void);

protected:
   VkPhysicalDeviceDescriptorIndexingFeatures _physicalDeviceDescriptorIndexingFeatures{};
   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddressFeatures{};
   VkPhysicalDeviceRayTracingPipelineFeaturesKHR _enabledRayTracingPipelineFeatures{};
   VkPhysicalDeviceAccelerationStructureFeaturesKHR _enabledAccelerationStructureFeatures{};
   VkPhysicalDeviceShaderClockFeaturesKHR _physicalDeviceShaderClockFeaturesKHR{};

   VkPipelineLayout _rasterizationPipelineLayout;
   VkPipeline _rasterizationPipeline;
   VkPipeline _rasterizationPipelineWireframe;

   VkPipeline _skyBoxRasterizationPipeline;
   VkPipeline _skyBoxRasterizationPipelineWireframe;

   genesis::Buffer* _sceneUbo;

   VkDescriptorSetLayout _rasterizationDescriptorSetLayout;
   VkDescriptorSet _rasterizationDescriptorSet;

   genesis::CellManager* _cellManager = nullptr;

   genesis::Image* _skyCubeMapImage = nullptr;
   genesis::Texture* _skyCubeMapTexture = nullptr;

   std::vector<genesis::Shader*> _shaders;

   // work around so as to use the same mechanism to render
   genesis::CellManager* _skyBoxManager = nullptr;
   VkPipelineLayout _rasterizationSkyBoxPipelineLayout;

   bool _wireframe = false;

   PushConstants _pushConstants;

   VkDescriptorPool _rasterizationDescriptorPool = VK_NULL_HANDLE;
};