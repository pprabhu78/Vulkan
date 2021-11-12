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
}

class Tutorial : public  VulkanExampleBase
{
public:
   Tutorial();
   ~Tutorial();

public:
   virtual void setupRenderPass() override;
   virtual void buildCommandBuffers() override;
   virtual void render() override;
   virtual void viewChanged() override;
   virtual void prepare() override;
   virtual void getEnabledFeatures() override;

public:
   virtual void draw();
   virtual void prepareSceneUbo();
   virtual void updateSceneUbo(void);
   
   virtual void setupDescriptorPool();
   virtual void setupDescriptorSetLayout(void);
   virtual void updateDescriptorSet(void);
   virtual void preparePipelines();

   virtual void loadAssets(void);

protected:
   genesis::Device* _device;

   VkPipelineLayout _pipelineLayout;
   VkPipeline _pipeline;

   genesis::Buffer* _sceneUbo;

   VkDescriptorSetLayout _setLayout0;
   VkDescriptorSet _descriptorSet0;

   genesis::VulkanGltfModel* _gltfModel;   

   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};

   VkPhysicalDeviceVulkan11Features _physicalDeviceVulkan11Features{};

   std::vector<genesis::Shader*> _shaders;
};