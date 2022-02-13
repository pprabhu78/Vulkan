#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace genesis
{
   class Device;
   class VulkanGltfModel;
   class Buffer;

   struct Node;

   class IndirectLayout
   {
   public:
      IndirectLayout(Device* device, bool rayTracing, bool indirect);
      virtual ~IndirectLayout();
   public:
      virtual void build(const VulkanGltfModel* model);
      virtual const std::vector<VkDescriptorSet>& descriptorSets(void) const;
      virtual VkDescriptorSetLayout vulkanDescriptorSetLayout(void) const;
      
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const;

   protected:
      virtual void setupDescriptorPool(const VulkanGltfModel* model);
      virtual void setupDescriptorSetLayout(const VulkanGltfModel* model);
      virtual void updateDescriptorSets(const VulkanGltfModel* model);
      virtual void createGpuSideBuffers(const VulkanGltfModel* model);
      virtual void destroyGpuSideBuffers(void);
      virtual void buildIndirectBuffer(const VulkanGltfModel* model);
      
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node* node, const VulkanGltfModel* model) const;

   protected:
      Device* _device;

      VkDescriptorPool _descriptorPool;
      VkDescriptorSetLayout _descriptorSetLayout;
      std::vector<VkDescriptorSet> _vecDescriptorSets;

      static const int s_maxBindlessTextures;

      const bool _indirect;

      const bool _rayTracing;

      std::vector<std::uint32_t> _indexIndices;

      //! Nodes to be rendered, correspond to meshes.
      //! The meshes have primitives
      //! The primitive refers to an index of the materials
      //! in the model. This index is that index for each rendered
      //! geometry (sub-mesh if you will)
      std::vector<std::uint32_t> _materialIndices;

      //! flattened list of indices into the index buffer
      Buffer* _indexIndicesGpu = nullptr;

      //! Gpu side material buffer (materials found in the gltf)
      Buffer* _materialsGpu = nullptr;

      //! flattened list of material indices
      Buffer* _materialIndicesGpu = nullptr;

      //! same buffer as above, but on the Gpu
      Buffer* _indirectBufferGpu;

      //! indirect command buffer
      std::vector<VkDrawIndexedIndirectCommand> _indirectCommands;

      static const int s_maxBindlessTextures;
   };
}
