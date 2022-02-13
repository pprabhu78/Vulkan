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

   class NonIndirectLayout
   {
   public:
      NonIndirectLayout(Device* device);
      virtual ~NonIndirectLayout();
   public:
      virtual void build(const VulkanGltfModel* model);
      virtual const std::vector<VkDescriptorSet>& descriptorSets(void) const;
      virtual VkDescriptorSetLayout vulkanDescriptorSetLayout(void) const;

      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const;

   protected:
      virtual void setupDescriptorPool(const VulkanGltfModel* model);
      virtual void setupDescriptorSetLayout(const VulkanGltfModel* model);
      virtual void updateDescriptorSets(const VulkanGltfModel* model);

      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node* node, const VulkanGltfModel* model) const;
   protected:
      Device* _device;

      VkDescriptorPool _descriptorPool;
      VkDescriptorSetLayout _descriptorSetLayout;
      std::vector<VkDescriptorSet> _vecDescriptorSets;

   };
}