#include "NonIndirectLayout.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "Texture.h"
#include "Buffer.h"
#include "VulkanDebug.h"

namespace genesis
{
   NonIndirectLayout::NonIndirectLayout(Device* device)
      : _device(device)
   {
      // nothing else to do
   }

   NonIndirectLayout::~NonIndirectLayout()
   {
      vkDestroyDescriptorPool(_device->vulkanDevice(), _descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _descriptorSetLayout, nullptr);
   }

   void NonIndirectLayout::build(const VulkanGltfModel* model)
   {
      setupDescriptorPool(model);
      setupDescriptorSetLayout(model);
      updateDescriptorSets(model);
   }

   void NonIndirectLayout::setupDescriptorPool(const VulkanGltfModel* model)
   {
      std::vector<VkDescriptorPoolSize> poolSizes = {};

      const std::vector<Texture*>& textures = model->textures();

      poolSizes.push_back(genesis::vkInitaliazers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)textures.size()));

      const uint32_t maxSets = (uint32_t)textures.size();

      VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::vkInitaliazers::descriptorPoolCreateInfo(poolSizes, maxSets);
      VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool));
   }

   void NonIndirectLayout::setupDescriptorSetLayout(const VulkanGltfModel* model)
   {
      std::vector<VkDescriptorSetLayoutBinding> setBindings = {};
      uint32_t bindingIndex = 0;

      VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;

      setBindings.push_back(genesis::vkInitaliazers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++));
      descriptorSetLayoutCreateInfo = genesis::vkInitaliazers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &descriptorSetLayoutCreateInfo, nullptr, &_descriptorSetLayout));
   }

   void NonIndirectLayout::updateDescriptorSets(const VulkanGltfModel* model)
   {
      const std::vector<Texture*>& textures = model->textures();
      for (int i = 0; i < textures.size(); ++i)
      {
         const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::vkInitaliazers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);

         // allocate a set
         VkDescriptorSet descriptorSet;
         vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

         std::vector<VkWriteDescriptorSet> writeDescriptorSets =
         {
         genesis::vkInitaliazers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures[i]->descriptor())
         };

         vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

         _vecDescriptorSets.push_back(descriptorSet);
      }
   }

   void NonIndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node* node, const VulkanGltfModel* model) const
   {
      if (node->_mesh == nullptr)
      {
         return;
      }

      const auto& materials = model->materials();
      for (const Primitive& primitive : node->_mesh->primitives)
      {
         if (primitive.indexCount > 0)
         {
            const Material& material = materials[primitive.materialIndex];
            const int textureIndex = material.baseColorTextureIndex;

            // Bind descriptor sets describing shader binding points
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &_vecDescriptorSets[textureIndex], 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
         }
      }

      // draw the children
      for (const auto& child : node->_children) {
         draw(commandBuffer, pipelineLayout, child, model);
      }
   }

   void NonIndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const
   {
      for (const Node* node : model->linearNodes())
      {
         draw(commandBuffer, pipelineLayout, node, model);
      }
   }
}