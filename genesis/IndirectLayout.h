#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <functional>
#include <tuple>

#include "InstanceContainer.h"

namespace genesis
{
   class Device;
   class VulkanGltfModel;
   class Buffer;
   class ModelRegistry;

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
      
      virtual void buildDrawBuffer(const ModelRegistry* modelRegistry, const InstanceContainer* instanceContainer);
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const;

   protected:
      virtual void setupDescriptorPool(int totalNumTextures);
      virtual void setupDescriptorSetLayout(int totalNumTextures);
      virtual void updateDescriptorSets(const std::vector<const VulkanGltfModel*>& models);
      virtual void createGpuSideBuffers(const std::vector<const VulkanGltfModel*>& models);
      virtual void destroyGpuSideBuffers(void);
      virtual void fillIndexAndMaterialIndices(const VulkanGltfModel* model);

      virtual void fillIndirectCommands(const VulkanGltfModel* model, int firstInstance, int instanceCount);
      virtual void createGpuSideDrawBuffers();
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

      //! indirect command buffer
      std::vector<VkDrawIndexedIndirectCommand> _indirectCommands;
      //! same buffer as above, but on the Gpu
      Buffer* _indirectBufferGpu = nullptr;

      //! instances
      std::vector<Instance> _flattenedInstances;
      //! same as above, but on the gpu
      Buffer* _flattenedInstancesGpu = nullptr;;

      //! flattened list of models
      std::vector<const VulkanGltfModel*> _flattenedModels;
      Buffer* _modelsGpu = nullptr;

      //! for each model, offset into the _indirectBufferGpu (in bytes) where that model starts
      //! And the size (equal to the num of primitives in the model)
      std::vector< std::tuple<int, int> > _modelDrawOffsetAndSize;      
   };
}
