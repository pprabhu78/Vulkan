#pragma once

#include "vulkanexamplebase.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class VulkanGltfModel;
   class Texture;
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

public:
   virtual void draw();
   virtual void prepareUbo();
   virtual void updateUbo(void);
   virtual void prepareTexture(void);
   virtual void setupDescriptorPool();
   virtual void setupDescriptorSetLayout(void);
   virtual void setupDescriptorSet(void);
   virtual void preparePipelines();

   virtual void loadAssets(void);

protected:
   VkPipelineLayout pipelineLayout;

   VkPipeline pipeline;

   genesis::Device* device;
   genesis::Buffer* uniformBuffer;

   VkDescriptorSetLayout descriptorSetLayout;

   VkDescriptorSet descriptorSet;

   genesis::Image* image;
   genesis::Texture* texture;

   genesis::VulkanGltfModel* gltfModel;
   
};