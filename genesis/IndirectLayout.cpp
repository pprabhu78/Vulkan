#include "IndirectLayout.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "Texture.h"
#include "Buffer.h"
#include "VulkanDebug.h"
#include <deque>

#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/common/gltfModelDesc.h"

namespace genesis
{
   IndirectLayout::IndirectLayout(Device* device)
      : _device(device)
      , _indirectBufferGpu(nullptr)
   {
      // nothing else to do
   }
   
   IndirectLayout::~IndirectLayout()
   {
      destroyGpuSideBuffers();

      vkDestroyDescriptorPool(_device->vulkanDevice(), _descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _descriptorSetLayout, nullptr);

      delete _indirectBufferGpu;
   }

   void IndirectLayout::createGpuSideBuffers(const std::vector<const VulkanGltfModel*>& gltfModels)
   {
      std::vector<ModelDesc> models;
      int currentTextureOffset = 0;
      for (const VulkanGltfModel* model : gltfModels)
      {
         // clear it before the next model
         _scratchMaterialIndices.clear();
         _scratchIndexIndices.clear();

         fillIndexAndMaterialIndices(model);

         const auto& materials = model->materials();

         const int sizeInBytesMaterialIndices = (int)(_scratchMaterialIndices.size() * sizeof(uint32_t));
         Buffer* materialIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterialIndices, true, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, "MaterialsIndicesGpu");
         void* pDstMaterialIndices = materialIndicesGpu->stagingBuffer();
         memcpy(pDstMaterialIndices, _scratchMaterialIndices.data(), sizeInBytesMaterialIndices);
         materialIndicesGpu->syncToGpu(true);
         _buffersCreatedHere.push_back(materialIndicesGpu);

         const int sizeInBytesIndexIndices = (int)(_scratchIndexIndices.size() * sizeof(uint32_t));
         Buffer* indexIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesIndexIndices, true, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, "IndexIndicesGpu");
         void* pDstIndexIndices = indexIndicesGpu->stagingBuffer();
         memcpy(pDstIndexIndices, _scratchIndexIndices.data(), sizeInBytesIndexIndices);
         indexIndicesGpu->syncToGpu(true);
         _buffersCreatedHere.push_back(indexIndicesGpu);

         const int sizeInBytesMaterials = (int)(materials.size() * sizeof(Material));
         Buffer* materialsGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterials, true, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, "MaterialsGpu");
         void* pDstMaterials = materialsGpu->stagingBuffer();
         memcpy(pDstMaterials, materials.data(), sizeInBytesMaterials);
         materialsGpu->syncToGpu(true);
         _buffersCreatedHere.push_back(materialsGpu);

         ModelDesc modelDesc;
         modelDesc.textureOffset = currentTextureOffset;
         currentTextureOffset += (int)model->textures().size();
         modelDesc.vertexBufferAddress = model->vertexBuffer()->bufferAddress();
         modelDesc.indexBufferAddress = model->indexBuffer()->bufferAddress();
         modelDesc.indexIndicesAddress = indexIndicesGpu->bufferAddress();
         modelDesc.materialAddress = materialsGpu->bufferAddress();
         modelDesc.materialIndicesAddress = materialIndicesGpu->bufferAddress();

         models.push_back(modelDesc);
      }

      const uint32_t sizeInBytesModels = (uint32_t)models.size() * sizeof(ModelDesc);

      _modelsGpu = new Buffer(_device, BT_SBO, sizeInBytesModels, true, 0, "ModelsGpu");
      void* pDstModels = _modelsGpu->stagingBuffer();
      memcpy(pDstModels, models.data(), sizeInBytesModels);
      _modelsGpu->syncToGpu(true);
   }

   void IndirectLayout::destroyGpuSideBuffers(void)
   {
      delete _modelsGpu;
      _modelsGpu = nullptr;

      for (Buffer* buffer : _buffersCreatedHere)
      {
         delete buffer;
      }
      _buffersCreatedHere.clear();

      delete _indirectBufferGpu;
      _indirectBufferGpu = nullptr;
   }

   void IndirectLayout::build(const std::vector<const VulkanGltfModel*>& gltfModels)
   {
      createGpuSideBuffers(gltfModels);

      int totalNumTextures = 0;
      for (const VulkanGltfModel* model : gltfModels)
      {
         totalNumTextures += (int)model->textures().size();
      }

      setupDescriptorPool(totalNumTextures);
      setupDescriptorSetLayout(totalNumTextures);
      updateDescriptorSets(gltfModels);
   }

   void IndirectLayout::setupDescriptorPool(int totalNumTextures)
   {
      std::vector<VkDescriptorPoolSize> poolSizes = {};

      poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1));
      poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalNumTextures));

      const uint32_t maxSets = 1;
      
      VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, maxSets);
      VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool));
   }

   void IndirectLayout::setupDescriptorSetLayout(int totalNumTextures)
   {
      std::vector<VkDescriptorSetLayoutBinding> setBindings = {};
      uint32_t bindingIndex = 0;

      VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;

      // model buffer
      setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
      // samplers
      setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, totalNumTextures));

      descriptorSetLayoutCreateInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

      // additional flags to specify the last variable binding point
      VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
      setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
      setLayoutBindingFlags.bindingCount = (uint32_t)setBindings.size();

      std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;

      // model buffer
      descriptorBindingFlags.push_back(0);
      // samplers
      descriptorBindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

      setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

      descriptorSetLayoutCreateInfo.pNext = &setLayoutBindingFlags;


      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &descriptorSetLayoutCreateInfo, nullptr, &_descriptorSetLayout));
   }

   static void writeAndUpdateDescriptorSet(VkDescriptorSet dstSet
      , uint32_t binding
      , const std::vector<const Texture*>& textures
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

   void IndirectLayout::updateDescriptorSets(const std::vector<const VulkanGltfModel*>& gltfModels)
   {
      std::vector<const Texture*> allTextures;
      for (const VulkanGltfModel* model : gltfModels)
      {
         const auto& textures = model->textures();
         for (const Texture* texture : textures)
         {
            allTextures.push_back(texture);
         }
      }

      uint32_t variableDescCounts[] = { uint32_t(allTextures.size()) };
      VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
      variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
      variableDescriptorCountAllocInfo.descriptorSetCount = 1;
      variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

      VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);
      descriptorSetAllocateInfo.pNext = &variableDescriptorCountAllocInfo;

      VkDescriptorSet descriptorSet;
      vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

      int bindingIndex = 0;
      std::vector<VkWriteDescriptorSet> writeDescriptorSets;
      writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, &_modelsGpu->descriptor()));

      vkUpdateDescriptorSets(_device->vulkanDevice(), (int)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

      writeAndUpdateDescriptorSet(descriptorSet, bindingIndex++, allTextures, _device->vulkanDevice());

      _vecDescriptorSets.push_back(descriptorSet);
   }

   void IndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const VulkanGltfModel* model) const
   {
      // Bind triangle vertex buffer (contains position and colors)
      VkDeviceSize offsets[1] = { 0 };
      VkBuffer buffer = model->vertexBuffer()->vulkanBuffer();
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, offsets);

      // Bind triangle index buffer
      vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer()->vulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

      std::uint32_t firstSet = 1;
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, std::uint32_t(_vecDescriptorSets.size()), _vecDescriptorSets.data(), 0, nullptr);
      vkCmdDrawIndexedIndirect(commandBuffer, _indirectBufferGpu->vulkanBuffer(), 0, (uint32_t)_indirectCommands.size(), sizeof(VkDrawIndexedIndirectCommand));
   }

   void IndirectLayout::fillIndexAndMaterialIndices(const VulkanGltfModel* model)
   {
      forEachPrimitive(model, 
         [&](const Primitive& primitive)
         {
            _scratchMaterialIndices.push_back(primitive.materialIndex);
            _scratchIndexIndices.push_back(primitive.firstIndex);
         }
      );
   }

   const std::vector<VkDescriptorSet>& IndirectLayout::descriptorSets(void) const
   {
      return _vecDescriptorSets;
   }

   VkDescriptorSetLayout IndirectLayout::vulkanDescriptorSetLayout(void) const
   {
      return _descriptorSetLayout;
   }


   void IndirectLayout::fillIndirectCommands(const VulkanGltfModel* model)
   {
      forEachPrimitive(model, [&](const Primitive& primitive)
         {
            VkDrawIndexedIndirectCommand command;
            command.indexCount = primitive.indexCount;
            command.instanceCount = 1;
            command.firstIndex = primitive.firstIndex;
            command.vertexOffset = 0;
            command.firstInstance = 0;
            _indirectCommands.push_back(command);

         });
   }

   void IndirectLayout::createGpuSideDrawBuffers()
   {
      const int sizeInBytes = (int)(_indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
      _indirectBufferGpu = new Buffer(_device, BT_INDIRECT_BUFFER, sizeInBytes, true);
      void* pDst = _indirectBufferGpu->stagingBuffer();
      memcpy(pDst, _indirectCommands.data(), sizeInBytes);
      _indirectBufferGpu->syncToGpu(true);
   }

   void IndirectLayout::buildDrawBuffers(const std::vector<const VulkanGltfModel*>& gltfModels)
   {
      _indirectCommands.clear();
      for (const VulkanGltfModel* model : gltfModels)
      {
         fillIndirectCommands(model);
      }
      createGpuSideDrawBuffers();
   }

   void IndirectLayout::forEachPrimitive(const VulkanGltfModel* model, const std::function<void(const Primitive&)>& func)
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
                  func(primitive);
               }
            }
         }

         for (const Node* child : node->_children)
         {
            nodesToProcess.push_back(child);
         }
      }
   }
}