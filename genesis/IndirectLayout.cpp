#include "IndirectLayout.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "Texture.h"
#include "Buffer.h"
#include "VulkanDebug.h"
#include <deque>

namespace genesis
{
   IndirectLayout::IndirectLayout(Device* device, bool rayTracing, bool indirect)
      : _device(device)
      , _rayTracing(rayTracing)
      , _indirect(indirect)
      , _materialIndicesGpu(nullptr)
      , _indexIndicesGpu(nullptr)
      , _materialsGpu(nullptr)
      , _indirectBufferGpu(nullptr)
   {
      // nothing else to do
   }
   
   IndirectLayout::~IndirectLayout()
   {
      destroyGpuSideBuffers();

      vkDestroyDescriptorPool(_device->vulkanDevice(), _descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _descriptorSetLayout, nullptr);
   }

   void IndirectLayout::createGpuSideBuffers(const VulkanGltfModel* model)
   {
      const auto& materials = model->materials();

      buildIndirectBuffer(model);

      const int sizeInBytesMaterialIndices = (int)(_materialIndices.size() * sizeof(uint32_t));
      _materialIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterialIndices, true);
      void* pDstMaterialIndices = _materialIndicesGpu->stagingBuffer();
      memcpy(pDstMaterialIndices, _materialIndices.data(), sizeInBytesMaterialIndices);
      _materialIndicesGpu->syncToGpu(true);

      const int sizeInBytesIndexIndices = (int)(_indexIndices.size() * sizeof(uint32_t));
      _indexIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesIndexIndices, true);
      void* pDstIndexIndices = _indexIndicesGpu->stagingBuffer();
      memcpy(pDstIndexIndices, _indexIndices.data(), sizeInBytesIndexIndices);
      _indexIndicesGpu->syncToGpu(true);

      const int sizeInBytesMaterials = (int)(materials.size() * sizeof(Material));
      _materialsGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterials, true, 0, "MaterialsGpu");
      void* pDstMaterials = _materialsGpu->stagingBuffer();
      memcpy(pDstMaterials, materials.data(), sizeInBytesMaterials);
      _materialsGpu->syncToGpu(true);
   }

   void IndirectLayout::destroyGpuSideBuffers(void)
   {
      delete _indexIndicesGpu;
      delete _materialIndicesGpu;
      delete _materialsGpu;
      delete _indirectBufferGpu;
   }

   void IndirectLayout::build(const VulkanGltfModel* model)
   {
      createGpuSideBuffers(model);

      setupDescriptorPool(model);
      setupDescriptorSetLayout(model);
      updateDescriptorSets(model);
   }

   const int IndirectLayout::s_maxBindlessTextures = 100;

   void IndirectLayout::setupDescriptorPool(const VulkanGltfModel* model)
   {
      std::vector<VkDescriptorPoolSize> poolSizes = {};

      const std::vector<Texture*>& textures = model->textures();

      uint32_t maxSets = 0;
      if (_indirect)
      {
         if (_rayTracing)
         { 
            poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3));
         }
         
         poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2));
         poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_maxBindlessTextures));

         maxSets = 1;
      }
      else
      {
         poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)textures.size()));

         maxSets = (uint32_t)textures.size();
      }

      VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, maxSets);
      VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool));
   }

   void IndirectLayout::setupDescriptorSetLayout(const VulkanGltfModel* model)
   {
      std::vector<VkDescriptorSetLayoutBinding> setBindings = {};
      uint32_t bindingIndex = 0;

      VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;

      if (_indirect)
      {
         if (_rayTracing)
         {
            // vertices
            setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++));
            // indices
            setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++));
            // index indices
            setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         }
         // material buffer
         setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         // material indices
         setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         // samplers
         setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, s_maxBindlessTextures));

         descriptorSetLayoutCreateInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

         // additional flags to specify the last variable binding point
         VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
         setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
         setLayoutBindingFlags.bindingCount = (uint32_t)setBindings.size();

         std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;
         if (_rayTracing)
         {
            // vertices
            descriptorBindingFlags.push_back(0);
            // indices
            descriptorBindingFlags.push_back(0);
            // index indices
            descriptorBindingFlags.push_back(0);
         }
         // material buffer
         descriptorBindingFlags.push_back(0);
         // material indices
         descriptorBindingFlags.push_back(0);
         // samplers
         descriptorBindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

         setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

         descriptorSetLayoutCreateInfo.pNext = &setLayoutBindingFlags;
      }
      else
      {
         setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++));
         descriptorSetLayoutCreateInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));
      }

      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &descriptorSetLayoutCreateInfo, nullptr, &_descriptorSetLayout));
   }

   static void writeAndUpdateDescriptorSet(VkDescriptorSet dstSet
      , uint32_t binding
      , const std::vector<Texture*>& textures
      , VkDevice device)
   {
      VkWriteDescriptorSet writeDescriptorSet{};
      std::vector<VkDescriptorImageInfo> textureDescriptors;
      textureDescriptors.reserve(textures.size());
      for (const Texture* texture : textures)
      {
         textureDescriptors.push_back(texture->descriptor());
      }

      writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSet.dstBinding = binding;
      writeDescriptorSet.dstArrayElement = 0;
      writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeDescriptorSet.descriptorCount = static_cast<uint32_t>(textures.size());
      writeDescriptorSet.pBufferInfo = 0;
      writeDescriptorSet.dstSet = dstSet;
      writeDescriptorSet.pImageInfo = textureDescriptors.data();

      vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
   }

   void IndirectLayout::updateDescriptorSets(const VulkanGltfModel* model)
   {
      if (_indirect)
      {
         uint32_t variableDescCounts[] = { s_maxBindlessTextures };
         VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
         variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
         variableDescriptorCountAllocInfo.descriptorSetCount = 1;
         variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

         VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);
         descriptorSetAllocateInfo.pNext = &variableDescriptorCountAllocInfo;

         VkDescriptorSet descriptorSet;
         vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

         const std::vector<Texture*>& textures = model->textures();

         int bindingIndex = 0;
         std::vector<VkWriteDescriptorSet> writeDescriptorSets;
         if (_rayTracing)
         {
            writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, model->vertexBuffer()->descriptorPtr()));
            writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, model->indexBuffer()->descriptorPtr()));
            writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, &_indexIndicesGpu->descriptor()));
         }
         writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, &_materialsGpu->descriptor()));
         writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, &_materialIndicesGpu->descriptor()));

         vkUpdateDescriptorSets(_device->vulkanDevice(), (int)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

         writeAndUpdateDescriptorSet(descriptorSet, bindingIndex++, textures, _device->vulkanDevice());

         _vecDescriptorSets.push_back(descriptorSet);
      }
      else
      {

         const std::vector<Texture*>& textures = model->textures();
         for (int i = 0; i < textures.size(); ++i)
         {
            const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);

            // allocate a set
            VkDescriptorSet descriptorSet;
            vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

            std::vector<VkWriteDescriptorSet> writeDescriptorSets =
            {
            genesis::VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures[i]->descriptor())
            };

            vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

            _vecDescriptorSets.push_back(descriptorSet);
         }
      }
   }

   void IndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node* node, const VulkanGltfModel* model) const
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

   void IndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const
   {
      // Bind triangle vertex buffer (contains position and colors)
      VkDeviceSize offsets[1] = { 0 };
      VkBuffer buffer = model->vertexBuffer()->vulkanBuffer();
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, offsets);

      // Bind triangle index buffer
      vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer()->vulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

      if (_indirect)
      {
         std::uint32_t firstSet = 1;
         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, std::uint32_t(_vecDescriptorSets.size()), _vecDescriptorSets.data(), 0, nullptr);
         vkCmdDrawIndexedIndirect(commandBuffer, _indirectBufferGpu->vulkanBuffer(), 0, (uint32_t)_indirectCommands.size(), sizeof(VkDrawIndexedIndirectCommand));
      }
      else
      {
         for (const Node* node : model->linearNodes())
         {
            draw(commandBuffer, pipelineLayout, node, model);
         }
      }
   }

   void IndirectLayout::buildIndirectBuffer(const VulkanGltfModel* model)
   {
      std::deque<const Node*> nodesToProcess;
      for (const Node* node : model->linearNodes())
      {
         nodesToProcess.push_back(node);
      }

      while (!nodesToProcess.empty())
      {
         const Node* node = nodesToProcess.front(); nodesToProcess.pop_front();

         if (node->_mesh)
         {
            for (const Primitive& primitive : node->_mesh->primitives)
            {
               if (primitive.indexCount > 0)
               {
                  VkDrawIndexedIndirectCommand command;
                  command.indexCount = primitive.indexCount;
                  command.instanceCount = 1;
                  command.firstIndex = primitive.firstIndex;
                  command.vertexOffset = 0;
                  command.firstInstance = 0;
                  _indirectCommands.push_back(command);
                  _materialIndices.push_back(primitive.materialIndex);
                  _indexIndices.push_back(primitive.firstIndex);
               }
            }
         }

         for (const Node* child : node->_children)
         {
            nodesToProcess.push_back(child);
         }
      }

      const int sizeInBytes = (int)(_indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
      _indirectBufferGpu = new Buffer(_device, BT_INDIRECT_BUFFER, sizeInBytes, true);
      void* pDst = _indirectBufferGpu->stagingBuffer();
      memcpy(pDst, _indirectCommands.data(), sizeInBytes);
      _indirectBufferGpu->syncToGpu(true);
   }

   const std::vector<VkDescriptorSet>& IndirectLayout::descriptorSets(void) const
   {
      return _vecDescriptorSets;
   }

   VkDescriptorSetLayout IndirectLayout::vulkanDescriptorSetLayout(void) const
   {
      return _descriptorSetLayout;
   }
}