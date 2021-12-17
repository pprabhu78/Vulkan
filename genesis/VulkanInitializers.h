#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace genesis
{
   class VulkanInitializers
   {
   public:
      static inline VkMemoryAllocateInfo memoryAllocateInfo()
      {
         VkMemoryAllocateInfo memAllocInfo{};
         memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
         return memAllocInfo;
      }

      static inline VkMappedMemoryRange mappedMemoryRange()
      {
         VkMappedMemoryRange mappedMemoryRange{};
         mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
         return mappedMemoryRange;
      }

      static inline VkCommandBufferAllocateInfo commandBufferAllocateInfo(
         VkCommandPool commandPool,
         VkCommandBufferLevel level,
         uint32_t bufferCount)
      {
         VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
         commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         commandBufferAllocateInfo.commandPool = commandPool;
         commandBufferAllocateInfo.level = level;
         commandBufferAllocateInfo.commandBufferCount = bufferCount;
         return commandBufferAllocateInfo;
      }

      static inline VkCommandPoolCreateInfo commandPoolCreateInfo()
      {
         VkCommandPoolCreateInfo cmdPoolCreateInfo{};
         cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         return cmdPoolCreateInfo;
      }

      static inline VkCommandBufferBeginInfo commandBufferBeginInfo()
      {
         VkCommandBufferBeginInfo cmdBufferBeginInfo{};
         cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
         return cmdBufferBeginInfo;
      }

      static inline VkCommandBufferInheritanceInfo commandBufferInheritanceInfo()
      {
         VkCommandBufferInheritanceInfo cmdBufferInheritanceInfo{};
         cmdBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
         return cmdBufferInheritanceInfo;
      }

      static inline VkRenderPassBeginInfo renderPassBeginInfo()
      {
         VkRenderPassBeginInfo renderPassBeginInfo{};
         renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
         return renderPassBeginInfo;
      }


      static inline VkRenderPassCreateInfo renderPassCreateInfo()
      {
         VkRenderPassCreateInfo renderPassCreateInfo{};
         renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
         return renderPassCreateInfo;
      }

      /** @brief Initialize an image memory barrier with no image transfer ownership */
      static inline VkImageMemoryBarrier imageMemoryBarrier()
      {
         VkImageMemoryBarrier imageMemoryBarrier{};
         imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         return imageMemoryBarrier;
      }

      /** @brief Initialize a buffer memory barrier with no image transfer ownership */
      static inline VkBufferMemoryBarrier bufferMemoryBarrier()
      {
         VkBufferMemoryBarrier bufferMemoryBarrier{};
         bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
         bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         return bufferMemoryBarrier;
      }

      static inline VkMemoryBarrier memoryBarrier()
      {
         VkMemoryBarrier memoryBarrier{};
         memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
         return memoryBarrier;
      }

      static inline VkImageCreateInfo imageCreateInfo()
      {
         VkImageCreateInfo imageCreateInfo{};
         imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
         return imageCreateInfo;
      }

      static inline VkSamplerCreateInfo samplerCreateInfo()
      {
         VkSamplerCreateInfo samplerCreateInfo{};
         samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
         samplerCreateInfo.maxAnisotropy = 1.0f;
         return samplerCreateInfo;
      }

      static inline VkImageViewCreateInfo imageViewCreateInfo()
      {
         VkImageViewCreateInfo imageViewCreateInfo{};
         imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
         return imageViewCreateInfo;
      }

      static inline VkFramebufferCreateInfo framebufferCreateInfo()
      {
         VkFramebufferCreateInfo framebufferCreateInfo{};
         framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         return framebufferCreateInfo;
      }

      static inline VkSemaphoreCreateInfo semaphoreCreateInfo()
      {
         VkSemaphoreCreateInfo semaphoreCreateInfo{};
         semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
         return semaphoreCreateInfo;
      }

      static inline VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0)
      {
         VkFenceCreateInfo fenceCreateInfo{};
         fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fenceCreateInfo.flags = flags;
         return fenceCreateInfo;
      }

      static inline VkEventCreateInfo eventCreateInfo()
      {
         VkEventCreateInfo eventCreateInfo{};
         eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
         return eventCreateInfo;
      }

      static inline VkSubmitInfo submitInfo()
      {
         VkSubmitInfo submitInfo{};
         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
         return submitInfo;
      }

      static inline VkViewport viewport(
         float width,
         float height,
         float minDepth,
         float maxDepth)
      {
         VkViewport viewport{};
         viewport.width = width;
         viewport.height = height;
         viewport.minDepth = minDepth;
         viewport.maxDepth = maxDepth;
         return viewport;
      }

      static inline VkRect2D rect2D(
         int32_t width,
         int32_t height,
         int32_t offsetX,
         int32_t offsetY)
      {
         VkRect2D rect2D{};
         rect2D.extent.width = width;
         rect2D.extent.height = height;
         rect2D.offset.x = offsetX;
         rect2D.offset.y = offsetY;
         return rect2D;
      }

      static inline VkBufferCreateInfo bufferCreateInfo()
      {
         VkBufferCreateInfo bufCreateInfo{};
         bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
         return bufCreateInfo;
      }

      static inline VkBufferCreateInfo bufferCreateInfo(
         VkBufferUsageFlags usage,
         VkDeviceSize size)
      {
         VkBufferCreateInfo bufCreateInfo{};
         bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
         bufCreateInfo.usage = usage;
         bufCreateInfo.size = size;
         return bufCreateInfo;
      }

      static inline VkDescriptorPoolCreateInfo descriptorPoolCreateInfo(
         uint32_t poolSizeCount,
         VkDescriptorPoolSize* pPoolSizes,
         uint32_t maxSets)
      {
         VkDescriptorPoolCreateInfo descriptorPoolInfo{};
         descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
         descriptorPoolInfo.poolSizeCount = poolSizeCount;
         descriptorPoolInfo.pPoolSizes = pPoolSizes;
         descriptorPoolInfo.maxSets = maxSets;
         return descriptorPoolInfo;
      }

      static inline VkDescriptorPoolCreateInfo descriptorPoolCreateInfo(
         const std::vector<VkDescriptorPoolSize>& poolSizes,
         uint32_t maxSets)
      {
         VkDescriptorPoolCreateInfo descriptorPoolInfo{};
         descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
         descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
         descriptorPoolInfo.pPoolSizes = poolSizes.data();
         descriptorPoolInfo.maxSets = maxSets;
         return descriptorPoolInfo;
      }

      static inline VkDescriptorPoolSize descriptorPoolSize(
         VkDescriptorType type,
         uint32_t descriptorCount)
      {
         VkDescriptorPoolSize descriptorPoolSize{};
         descriptorPoolSize.type = type;
         descriptorPoolSize.descriptorCount = descriptorCount;
         return descriptorPoolSize;
      }

      static inline VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(
         VkDescriptorType type,
         VkShaderStageFlags stageFlags,
         uint32_t binding,
         uint32_t descriptorCount = 1)
      {
         VkDescriptorSetLayoutBinding setLayoutBinding{};
         setLayoutBinding.descriptorType = type;
         setLayoutBinding.stageFlags = stageFlags;
         setLayoutBinding.binding = binding;
         setLayoutBinding.descriptorCount = descriptorCount;
         return setLayoutBinding;
      }

      static inline VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
         const VkDescriptorSetLayoutBinding* pBindings,
         uint32_t bindingCount)
      {
         VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
         descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
         descriptorSetLayoutCreateInfo.pBindings = pBindings;
         descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
         return descriptorSetLayoutCreateInfo;
      }

      static inline VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
         const std::vector<VkDescriptorSetLayoutBinding>& bindings)
      {
         VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
         descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
         descriptorSetLayoutCreateInfo.pBindings = bindings.data();
         descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
         return descriptorSetLayoutCreateInfo;
      }

      static inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
         const VkDescriptorSetLayout* pSetLayouts,
         uint32_t setLayoutCount = 1)
      {
         VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
         pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
         pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
         pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
         return pipelineLayoutCreateInfo;
      }

      static inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
         uint32_t setLayoutCount = 1)
      {
         VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
         pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
         pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
         return pipelineLayoutCreateInfo;
      }

      static inline VkDescriptorSetAllocateInfo descriptorSetAllocateInfo(
         VkDescriptorPool descriptorPool,
         const VkDescriptorSetLayout* pSetLayouts,
         uint32_t descriptorSetCount)
      {
         VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
         descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
         descriptorSetAllocateInfo.descriptorPool = descriptorPool;
         descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
         descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
         return descriptorSetAllocateInfo;
      }

      static inline VkDescriptorImageInfo descriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
      {
         VkDescriptorImageInfo descriptorImageInfo{};
         descriptorImageInfo.sampler = sampler;
         descriptorImageInfo.imageView = imageView;
         descriptorImageInfo.imageLayout = imageLayout;
         return descriptorImageInfo;
      }

      static inline VkWriteDescriptorSet writeDescriptorSet(
         VkDescriptorSet dstSet,
         VkDescriptorType type,
         uint32_t binding,
         const VkDescriptorBufferInfo* bufferInfo,
         uint32_t descriptorCount = 1)
      {
         VkWriteDescriptorSet writeDescriptorSet{};
         writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         writeDescriptorSet.dstSet = dstSet;
         writeDescriptorSet.descriptorType = type;
         writeDescriptorSet.dstBinding = binding;
         writeDescriptorSet.pBufferInfo = bufferInfo;
         writeDescriptorSet.descriptorCount = descriptorCount;
         return writeDescriptorSet;
      }

      static inline VkWriteDescriptorSet writeDescriptorSet(
         VkDescriptorSet dstSet,
         VkDescriptorType type,
         uint32_t binding,
         const VkDescriptorImageInfo* imageInfo,
         uint32_t descriptorCount = 1)
      {
         VkWriteDescriptorSet writeDescriptorSet{};
         writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         writeDescriptorSet.dstSet = dstSet;
         writeDescriptorSet.descriptorType = type;
         writeDescriptorSet.dstBinding = binding;
         writeDescriptorSet.pImageInfo = imageInfo;
         writeDescriptorSet.descriptorCount = descriptorCount;
         return writeDescriptorSet;
      }

      static inline VkVertexInputBindingDescription vertexInputBindingDescription(
         uint32_t binding,
         uint32_t stride,
         VkVertexInputRate inputRate)
      {
         VkVertexInputBindingDescription vInputBindDescription{};
         vInputBindDescription.binding = binding;
         vInputBindDescription.stride = stride;
         vInputBindDescription.inputRate = inputRate;
         return vInputBindDescription;
      }

      static inline VkVertexInputAttributeDescription vertexInputAttributeDescription(
         uint32_t binding,
         uint32_t location,
         VkFormat format,
         uint32_t offset)
      {
         VkVertexInputAttributeDescription vInputAttribDescription{};
         vInputAttribDescription.location = location;
         vInputAttribDescription.binding = binding;
         vInputAttribDescription.format = format;
         vInputAttribDescription.offset = offset;
         return vInputAttribDescription;
      }

      static inline VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo()
      {
         VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
         pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
         return pipelineVertexInputStateCreateInfo;
      }

      static inline VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
         const std::vector<VkVertexInputBindingDescription>& vertexBindingDescriptions,
         const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions
      )
      {
         VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
         pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
         pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size());
         pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions.data();
         pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
         pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
         return pipelineVertexInputStateCreateInfo;
      }

      static inline VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
         VkPrimitiveTopology topology,
         VkPipelineInputAssemblyStateCreateFlags flags,
         VkBool32 primitiveRestartEnable)
      {
         VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
         pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
         pipelineInputAssemblyStateCreateInfo.topology = topology;
         pipelineInputAssemblyStateCreateInfo.flags = flags;
         pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
         return pipelineInputAssemblyStateCreateInfo;
      }

      static inline VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
         VkPolygonMode polygonMode,
         VkCullModeFlags cullMode,
         VkFrontFace frontFace,
         VkPipelineRasterizationStateCreateFlags flags = 0)
      {
         VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
         pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
         pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
         pipelineRasterizationStateCreateInfo.cullMode = cullMode;
         pipelineRasterizationStateCreateInfo.frontFace = frontFace;
         pipelineRasterizationStateCreateInfo.flags = flags;
         pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
         pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
         return pipelineRasterizationStateCreateInfo;
      }

      static inline VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
         VkColorComponentFlags colorWriteMask,
         VkBool32 blendEnable)
      {
         VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
         pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
         pipelineColorBlendAttachmentState.blendEnable = blendEnable;
         return pipelineColorBlendAttachmentState;
      }

      static inline VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
         uint32_t attachmentCount,
         const VkPipelineColorBlendAttachmentState* pAttachments)
      {
         VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
         pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
         pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
         pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
         return pipelineColorBlendStateCreateInfo;
      }

      static inline VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
         VkBool32 depthTestEnable,
         VkBool32 depthWriteEnable,
         VkCompareOp depthCompareOp)
      {
         VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
         pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
         pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
         pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
         pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
         pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
         return pipelineDepthStencilStateCreateInfo;
      }

      static inline VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
         uint32_t viewportCount,
         uint32_t scissorCount,
         VkPipelineViewportStateCreateFlags flags = 0)
      {
         VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
         pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
         pipelineViewportStateCreateInfo.viewportCount = viewportCount;
         pipelineViewportStateCreateInfo.scissorCount = scissorCount;
         pipelineViewportStateCreateInfo.flags = flags;
         return pipelineViewportStateCreateInfo;
      }

      static inline VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
         VkSampleCountFlagBits rasterizationSamples,
         VkPipelineMultisampleStateCreateFlags flags = 0)
      {
         VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
         pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
         pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
         pipelineMultisampleStateCreateInfo.flags = flags;
         return pipelineMultisampleStateCreateInfo;
      }

      static inline VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
         const VkDynamicState* pDynamicStates,
         uint32_t dynamicStateCount,
         VkPipelineDynamicStateCreateFlags flags = 0)
      {
         VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
         pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
         pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
         pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
         pipelineDynamicStateCreateInfo.flags = flags;
         return pipelineDynamicStateCreateInfo;
      }

      static inline VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
         const std::vector<VkDynamicState>& pDynamicStates,
         VkPipelineDynamicStateCreateFlags flags = 0)
      {
         VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
         pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
         pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
         pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
         pipelineDynamicStateCreateInfo.flags = flags;
         return pipelineDynamicStateCreateInfo;
      }

      static inline VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
      {
         VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo{};
         pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
         pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
         return pipelineTessellationStateCreateInfo;
      }

      static inline VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
         VkPipelineLayout layout,
         VkRenderPass renderPass,
         VkPipelineCreateFlags flags = 0)
      {
         VkGraphicsPipelineCreateInfo info{};
         info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
         info.layout = layout;
         info.renderPass = renderPass;
         info.flags = flags;
         info.basePipelineIndex = -1;
         info.basePipelineHandle = VK_NULL_HANDLE;
         return info;
      }

      static inline VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo()
      {
         VkGraphicsPipelineCreateInfo info{};
         info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
         info.basePipelineIndex = -1;
         info.basePipelineHandle = VK_NULL_HANDLE;
         return info;
      }

      static inline VkComputePipelineCreateInfo computePipelineCreateInfo(
         VkPipelineLayout layout,
         VkPipelineCreateFlags flags = 0)
      {
         VkComputePipelineCreateInfo computePipelineCreateInfo{};
         computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
         computePipelineCreateInfo.layout = layout;
         computePipelineCreateInfo.flags = flags;
         return computePipelineCreateInfo;
      }

      static inline VkPushConstantRange pushConstantRange(
         VkShaderStageFlags stageFlags,
         uint32_t size,
         uint32_t offset)
      {
         VkPushConstantRange pushConstantRange{};
         pushConstantRange.stageFlags = stageFlags;
         pushConstantRange.offset = offset;
         pushConstantRange.size = size;
         return pushConstantRange;
      }

      static inline VkBindSparseInfo bindSparseInfo()
      {
         VkBindSparseInfo bindSparseInfo{};
         bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
         return bindSparseInfo;
      }

      /** @brief Initialize a map entry for a shader specialization constant */
      static inline VkSpecializationMapEntry specializationMapEntry(uint32_t constantID, uint32_t offset, size_t size)
      {
         VkSpecializationMapEntry specializationMapEntry{};
         specializationMapEntry.constantID = constantID;
         specializationMapEntry.offset = offset;
         specializationMapEntry.size = size;
         return specializationMapEntry;
      }

      /** @brief Initialize a specialization constant info structure to pass to a shader stage */
      static inline VkSpecializationInfo specializationInfo(uint32_t mapEntryCount, const VkSpecializationMapEntry* mapEntries, size_t dataSize, const void* data)
      {
         VkSpecializationInfo specializationInfo{};
         specializationInfo.mapEntryCount = mapEntryCount;
         specializationInfo.pMapEntries = mapEntries;
         specializationInfo.dataSize = dataSize;
         specializationInfo.pData = data;
         return specializationInfo;
      }

      /** @brief Initialize a specialization constant info structure to pass to a shader stage */
      static inline VkSpecializationInfo specializationInfo(const std::vector<VkSpecializationMapEntry>& mapEntries, size_t dataSize, const void* data)
      {
         VkSpecializationInfo specializationInfo{};
         specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
         specializationInfo.pMapEntries = mapEntries.data();
         specializationInfo.dataSize = dataSize;
         specializationInfo.pData = data;
         return specializationInfo;
      }

      // Ray tracing related
      static inline VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR()
      {
         VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR{};
         accelerationStructureGeometryKHR.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
         return accelerationStructureGeometryKHR;
      }

      static inline VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR()
      {
         VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR{};
         accelerationStructureBuildGeometryInfoKHR.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
         return accelerationStructureBuildGeometryInfoKHR;
      }

      static inline VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfoKHR()
      {
         VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfoKHR{};
         accelerationStructureBuildSizesInfoKHR.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
         return accelerationStructureBuildSizesInfoKHR;
      }

      static inline VkRayTracingShaderGroupCreateInfoKHR rayTracingShaderGroupCreateInfoKHR()
      {
         VkRayTracingShaderGroupCreateInfoKHR rayTracingShaderGroupCreateInfoKHR{};
         rayTracingShaderGroupCreateInfoKHR.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
         return rayTracingShaderGroupCreateInfoKHR;
      }

      static inline VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfoKHR()
      {
         VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfoKHR{};
         rayTracingPipelineCreateInfoKHR.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
         return rayTracingPipelineCreateInfoKHR;
      }

      static inline VkWriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAccelerationStructureKHR()
      {
         VkWriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAccelerationStructureKHR{};
         writeDescriptorSetAccelerationStructureKHR.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
         return writeDescriptorSetAccelerationStructureKHR;
      }
   };
}