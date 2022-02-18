#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <functional>

namespace genesis
{
   class Device;
   class VulkanGltfModel;
   class Buffer;

   struct Node;
   struct Primitive;

   class IndirectLayout
   {
   public:
      IndirectLayout(Device* device);
      virtual ~IndirectLayout();
   public:
      virtual void build(const std::vector<const VulkanGltfModel*>& models);
      virtual const std::vector<VkDescriptorSet>& descriptorSets(void) const;
      virtual VkDescriptorSetLayout vulkanDescriptorSetLayout(void) const;
      
      virtual void buildDrawBuffers(const std::vector<const VulkanGltfModel*>& models);
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const;

   protected:
      virtual void setupDescriptorPool(int totalNumTextures);
      virtual void setupDescriptorSetLayout(int totalNumTextures);
      virtual void updateDescriptorSets(const std::vector<const VulkanGltfModel*>& models);
      virtual void createGpuSideBuffers(const std::vector<const VulkanGltfModel*>& models);
      virtual void destroyGpuSideBuffers(void);
      virtual void fillIndexAndMaterialIndices(const VulkanGltfModel* model);

      virtual void fillIndirectCommands(const VulkanGltfModel* model);
      virtual void createGpuSideDrawBuffers();

      virtual void forEachPrimitive(const VulkanGltfModel* model, const std::function<void(const Primitive&)>& func);
   protected:
      Device* _device;

      VkDescriptorPool _descriptorPool;
      VkDescriptorSetLayout _descriptorSetLayout;
      std::vector<VkDescriptorSet> _vecDescriptorSets;

      std::vector<std::uint32_t> _scratchIndexIndices;

      //! Nodes to be rendered, correspond to meshes.
      //! The meshes have primitives
      //! The primitive refers to an index of the materials
      //! in the model. This index is that index for each rendered
      //! geometry (sub-mesh if you will)
      std::vector<std::uint32_t> _scratchMaterialIndices;

      //! flattened list of indices into the index buffer
      std::vector<Buffer*> _buffersCreatedHere;

      //! same buffer as above, but on the Gpu
      Buffer* _indirectBufferGpu;

      //! indirect command buffer
      std::vector<VkDrawIndexedIndirectCommand> _indirectCommands;

      Buffer* _modelsGpu = nullptr;
   };
}
