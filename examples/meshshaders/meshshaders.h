#pragma once

#include "../genesis/PlatformApplication.h"

#include "GenMath.h"
#include "Camera.h"

#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/common/sceneUbo.h"

namespace genesis
{
   class Device;
   class Buffer;
   class Image;
   class Texture;
   class VulkanMeshletModel;
   class Texture;
   class Shader;
   class RenderPass;
   class VulkanBuffer;
   class CellManager;
}

class MeshShaders : public genesis::PlatformApplication
{
public:
   MeshShaders();
   virtual ~MeshShaders();

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
   
protected:
   virtual void draw();

   virtual void createSceneUbo();
   virtual void updateSceneUbo();

   virtual void createScene();

   virtual void saveScreenShot(const std::string& fileName);
   virtual void resetCamera(void);
   std::string generateTimeStampedFileName(void);

   virtual void drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
   virtual void destroyRasterizationStuff();
   virtual void destroyCommonStuff();

   virtual void createAndUpdateDescriptorSets();
   virtual void createAndUpdateRasterizationDescriptorSets();
   virtual void createAndUpdateMeshShaderDescriptorSets();

   virtual void createPipelines();
   virtual void createRasterizationPipeline();

   virtual void buildRasterizationCommandBuffers(void);

   virtual void reloadShaders(bool destroyExistingStuff);
protected:

   VkPhysicalDeviceBufferDeviceAddressFeatures _enabledBufferDeviceAddressFeatures{};
   VkPhysicalDeviceMeshShaderFeaturesEXT _physicalDeviceMeshShaderFeaturesEXT{};
   VkPhysicalDeviceMaintenance4Features _physicalDeviceMaintenance4Features{};
   VkPhysicalDeviceDescriptorIndexingFeaturesEXT _physicalDeviceDescriptorIndexingFeatures{};

   // Rasterization
   VkPipelineLayout _rasterizationPipelineLayout;
   VkPipeline _rasterizationPipeline;

   VkDescriptorSetLayout _rasterizationDescriptorSetLayout;
   VkDescriptorSet _rasterizationDescriptorSet;
   VkDescriptorPool _rasterizationDescriptorPool = VK_NULL_HANDLE;

   // Mesh Shaders
   VkDescriptorSetLayout _meshShadersDescriptorSetLayout;
   VkDescriptorSet _meshShadersDescriptorSet;
   VkDescriptorPool _meshShadersDescriptorPool = VK_NULL_HANDLE;

   // Cube map
   VkPipeline _skyBoxRasterizationPipeline;
   genesis::Image* _skyCubeMapImage = nullptr;
   genesis::Texture* _skyCubeMapTexture = nullptr;

   // work around so as to use the same mechanism to render
   genesis::CellManager* _skyBoxManager = nullptr;
   VkPipelineLayout _rasterizationSkyBoxPipelineLayout;
   
   // common
   genesis::Buffer* _sceneUbo;
   PushConstants _pushConstants;

   genesis::VulkanMeshletModel * _model = nullptr;
};
