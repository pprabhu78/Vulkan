#include "IndirectLayout.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "Texture.h"
#include "Buffer.h"
#include "VulkanDebug.h"
#include "ModelInfo.h"
#include "ModelRegistry.h"
#include "InstanceContainer.h"

#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/common/gltfModelDesc.h"

#include <deque>
#include <utility>

namespace genesis
{
   IndirectLayout::IndirectLayout(Device* device)
      : _device(device)
   {
      // nothing else to do
   }
   
   IndirectLayout::~IndirectLayout()
   {
      destroyGpuSideBuffers();

      vkDestroyDescriptorPool(_device->vulkanDevice(), _descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _descriptorSetLayout, nullptr);
   }

   template<class T>
   Buffer* createFillAndPush(const std::vector<T>& src, BufferType bufferType, const std::string& name, Device* device, std::vector<Buffer*>& buffers)
   {
      const int sizeInBytes = (int)(src.size() * sizeof(T));
      Buffer* buffer = new Buffer(device, bufferType, sizeInBytes, true, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, name);
      void* pDst = buffer->stagingBuffer();
      memcpy(pDst, src.data(), sizeInBytes);
      buffer->syncToGpu(true);

      buffers.push_back(buffer);

      return buffer;
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

         Buffer* materialIndicesGpu = createFillAndPush(_scratchMaterialIndices, BT_SBO, "MaterialsIndicesGpu", _device, _buffersCreatedHere);
         Buffer* indexIndicesGpu = createFillAndPush(_scratchIndexIndices, BT_SBO, "IndexIndicesGpu", _device, _buffersCreatedHere);
         Buffer* materialsGpu = createFillAndPush(materials, BT_SBO, "MaterialsGpu", _device, _buffersCreatedHere);

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

      _modelsGpu = createFillAndPush(models, BT_SBO, "ModelsGpu", _device, _buffersCreatedHere);
   }

   void IndirectLayout::destroyGpuSideBuffers(void)
   {
      for (Buffer* buffer : _buffersCreatedHere)
      {
         delete buffer;
      }
      _buffersCreatedHere.clear();
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

      poolSizes.push_back(genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2));
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

      VkShaderStageFlags rayTracingFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
      VkShaderStageFlags rasterizationFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      // model buffer
      setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rayTracingFlags | rasterizationFlags, bindingIndex++, 1));
      // instances buffer
      setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rayTracingFlags | rasterizationFlags, bindingIndex++, 1));
      // samplers
      setBindings.push_back(genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, rayTracingFlags | rasterizationFlags, bindingIndex++, totalNumTextures));

      descriptorSetLayoutCreateInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

      // additional flags to specify the last variable binding point
      VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
      setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
      setLayoutBindingFlags.bindingCount = (uint32_t)setBindings.size();

      std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;

      // model buffer
      descriptorBindingFlags.push_back(0);
      // instances buffer
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
      writeDescriptorSets.push_back(VulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, &_flattenedInstancesGpu->descriptor()));

      vkUpdateDescriptorSets(_device->vulkanDevice(), (int)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

      writeAndUpdateDescriptorSet(descriptorSet, bindingIndex++, allTextures, _device->vulkanDevice());

      _vecDescriptorSets.push_back(descriptorSet);
   }

   void IndirectLayout::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
   {
      for (int i = 0; i < _flattenedModels.size(); ++i)
      {
         const VulkanGltfModel* model = _flattenedModels[i];
         // Bind triangle vertex buffer (contains position and colors)
         VkDeviceSize offsets[1] = { 0 };
         VkBuffer buffer = model->vertexBuffer()->vulkanBuffer();
         vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, offsets);

         // Bind triangle index buffer
         vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer()->vulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

         std::uint32_t firstSet = 1;
         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, std::uint32_t(_vecDescriptorSets.size()), _vecDescriptorSets.data(), 0, nullptr);
         vkCmdDrawIndexedIndirect(commandBuffer, _indirectBufferGpu->vulkanBuffer(), std::get<0>(_modelDrawOffsetAndSize[i]), std::get<1>(_modelDrawOffsetAndSize[i]), sizeof(VkDrawIndexedIndirectCommand));
      }
   }

   void IndirectLayout::fillIndexAndMaterialIndices(const VulkanGltfModel* model)
   {
      model->forEachPrimitive(
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

   void IndirectLayout::createGpuSideDrawBuffers()
   {
      _indirectBufferGpu = createFillAndPush(_indirectCommands, BT_INDIRECT_BUFFER, "IndirectBufferGpu", _device, _buffersCreatedHere);
      _flattenedInstancesGpu = createFillAndPush(_flattenedInstances, BT_SBO, "FlattenedInstances", _device, _buffersCreatedHere);
   }

   void IndirectLayout::fillIndirectCommands(const VulkanGltfModel* model, int firstInstance, int instanceCount)
   {
      int numFilled = 0;
         model->forEachPrimitive([&](const Primitive& primitive)
         {
            VkDrawIndexedIndirectCommand command;
            command.indexCount = primitive.indexCount;
            command.instanceCount = instanceCount;
            command.firstIndex = primitive.firstIndex;
            command.vertexOffset = 0;
            command.firstInstance = firstInstance;
            _indirectCommands.push_back(command);
            ++numFilled;
         });
         if (numFilled != model->numPrimitives())
         {
            std::cout << __FUNCTION__ << "warning: " << "numFilled != model->numPrimitives()" << std::endl;
         }
   }

   void IndirectLayout::buildDrawBuffer(const ModelRegistry* modelRegistry, const InstanceContainer* instanceContainer)
   {
      const auto& mapModelIdsToInstances = instanceContainer->mapModelIdsToInstances();
      const auto& instances = instanceContainer->instances();

      // Models have been registered in order with a running model id that's incremented
      // so, push them in order onto this vector
      int firstInstanceForThisModel = 0;
      int drawCommandOffset = 0;
      for (int i = 0; i < modelRegistry->numModels(); ++i)
      {
         const ModelInfo* modelInfo = modelRegistry->findModel(i);

         auto instanceIter = mapModelIdsToInstances.find(modelInfo->modelId());
         if (instanceIter == mapModelIdsToInstances.end())
         {
            std::cout << __FUNCTION__ << "warning: " << "instanceIter == mapModelIdsToInstances.end()" << std::endl;
            continue;
         }

         // Put the instances of this model in order
         const auto& setOfInstancesForThisModel = instanceIter->second;
         for (int instanceIndex : setOfInstancesForThisModel)
         {
            _flattenedInstances.push_back(instances[instanceIndex]);
         }

         fillIndirectCommands(modelInfo->model(), firstInstanceForThisModel, (uint32_t)setOfInstancesForThisModel.size());

         firstInstanceForThisModel += (int)setOfInstancesForThisModel.size();

         _flattenedModels.push_back(modelInfo->model());

         _modelDrawOffsetAndSize.push_back({ (int)(drawCommandOffset*sizeof(VkDrawIndexedIndirectCommand)), (int)modelInfo->model()->numPrimitives() });

         drawCommandOffset += modelInfo->model()->numPrimitives();
      }

      createGpuSideDrawBuffers();
   }
}