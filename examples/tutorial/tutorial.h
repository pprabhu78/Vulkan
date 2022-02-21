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

   virtual void buildCommandBuffers() override;
   virtual void render() override;
   virtual void viewChanged() override;
   virtual void prepare() override;
   virtual void enableFeatures() override;
   virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay) override;

public:
   virtual void draw();
   virtual void createSceneUbo();
   virtual void updateSceneUbo(void);
   
   virtual void setupDescriptorPool();
   virtual void setupDescriptorSetLayout(void);
   virtual void updateDescriptorSet(void);
   virtual void createRasterizationPipeline();

   virtual void createScene(void);

protected:

   VkPipelineLayout _pipelineLayout;
   VkPipeline _pipeline;
   VkPipeline _pipelineWireframe;

   VkPipeline _skyBoxPipeline;
   VkPipeline _skyBoxPipelineWireframe;

   genesis::Buffer* _sceneUbo;

   VkDescriptorSetLayout _setLayout0;
   VkDescriptorSet _descriptorSet0;

   genesis::CellManager* _cellManager = nullptr;

   genesis::VulkanGltfModel* _gltfSkyboxModel = nullptr;
   genesis::Image* _skyCubeMapImage = nullptr;
   genesis::Texture* _skyCubeMapTexture = nullptr;

   VkPhysicalDeviceDescriptorIndexingFeatures _physicalDeviceDescriptorIndexingFeatures{};

   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddressFeatures{};

   VkPhysicalDeviceVulkan11Features _physicalDeviceVulkan11Features{};

   std::vector<genesis::Shader*> _shaders;

   bool _wireframe = false;

   PushConstants _pushConstants;

#if 1
   genesis::IndirectLayout* _skyBoxLayout = nullptr;
#endif
};