/*
* Vulkan Example - Basic hardware accelerated ray tracing example
*
* Copyright (C) 2019-2020 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Shader.h"
#include "Device.h"
#include "tutorial_raytracing.h"

#include "VulkanInitializers.h"
#include "VulkanFunctions.h"
#include "VulkanGltf.h"
#include "AccelerationStructure.h"
#include "Buffer.h"

#define VENUS 0
#define SPONZA 1

TutorialRayTracing::TutorialRayTracing()
   : _gltfModel(nullptr)
   , _device(nullptr)
{
   title = "Ray tracing basic";
   settings.overlay = false;
   camera.type = Camera::CameraType::lookat;
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
   camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
   camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.5f));


#if VENUS
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -8.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
#endif

#if SPONZA
   camera.type = Camera::CameraType::firstperson;
   camera.rotationSpeed = 0.2f;
   camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
   camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
#endif

   // Require Vulkan 1.2
   apiVersion = VK_API_VERSION_1_2;

   // Ray tracing related extensions required by this sample
   enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

   // Required by VK_KHR_acceleration_structure
   enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

   // Required for VK_KHR_ray_tracing_pipeline
   enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

   // Required by VK_KHR_spirv_1_4
   enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

   // For descriptor indexing
   enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

}

TutorialRayTracing::~TutorialRayTracing()
{
   vkDestroyPipeline(device, pipeline, nullptr);
   vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
   vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

   vkDestroyImageView(device, storageImage.view, nullptr);
   vkDestroyImage(device, storageImage.image, nullptr);
   vkFreeMemory(device, storageImage.memory, nullptr);

   delete topLevelAS;

   transformBuffer.destroy();
   
   raygenShaderBindingTable.destroy();
   missShaderBindingTable.destroy();
   hitShaderBindingTable.destroy();
   
   delete _gltfModel;
   delete _sceneUbo;
}

/*
Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
*/
RayTracingScratchBuffer TutorialRayTracing::createScratchBuffer(VkDeviceSize size)
{
   RayTracingScratchBuffer scratchBuffer{};

   VkBufferCreateInfo bufferCreateInfo{};
   bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferCreateInfo.size = size;
   bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
   VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &scratchBuffer.handle));

   VkMemoryRequirements memoryRequirements{};
   vkGetBufferMemoryRequirements(device, scratchBuffer.handle, &memoryRequirements);

   VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
   memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
   memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

   VkMemoryAllocateInfo memoryAllocateInfo = {};
   memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
   memoryAllocateInfo.allocationSize = memoryRequirements.size;
   memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &scratchBuffer.memory));
   VK_CHECK_RESULT(vkBindBufferMemory(device, scratchBuffer.handle, scratchBuffer.memory, 0));

   VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
   bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
   bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
   scratchBuffer.deviceAddress = genesis::vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);

   return scratchBuffer;
}

void TutorialRayTracing::deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer)
{
   if (scratchBuffer.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, scratchBuffer.memory, nullptr);
   }
   if (scratchBuffer.handle != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, scratchBuffer.handle, nullptr);
   }
}

/*
Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/
uint64_t TutorialRayTracing::getBufferDeviceAddress(VkBuffer buffer)
{
   VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
   bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
   bufferDeviceAI.buffer = buffer;
   return genesis::vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
}

/*
Set up a storage image that the ray generation shader will be writing to
*/
void TutorialRayTracing::createStorageImage()
{
   VkImageCreateInfo image = genesis::vulkanInitializers::imageCreateInfo();
   image.imageType = VK_IMAGE_TYPE_2D;
   image.format = swapChain.colorFormat;
   image.extent.width = width;
   image.extent.height = height;
   image.extent.depth = 1;
   image.mipLevels = 1;
   image.arrayLayers = 1;
   image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &storageImage.image));

   VkMemoryRequirements memReqs;
   vkGetImageMemoryRequirements(device, storageImage.image, &memReqs);
   VkMemoryAllocateInfo memoryAllocateInfo = genesis::vulkanInitializers::memoryAllocateInfo();
   memoryAllocateInfo.allocationSize = memReqs.size;
   memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &storageImage.memory));
   VK_CHECK_RESULT(vkBindImageMemory(device, storageImage.image, storageImage.memory, 0));

   VkImageViewCreateInfo colorImageView = genesis::vulkanInitializers::imageViewCreateInfo();
   colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
   colorImageView.format = swapChain.colorFormat;
   colorImageView.subresourceRange = {};
   colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   colorImageView.subresourceRange.baseMipLevel = 0;
   colorImageView.subresourceRange.levelCount = 1;
   colorImageView.subresourceRange.baseArrayLayer = 0;
   colorImageView.subresourceRange.layerCount = 1;
   colorImageView.image = storageImage.image;
   VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &storageImage.view));

   VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
   vks::tools::setImageLayout(cmdBuffer, storageImage.image,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_GENERAL,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
   vulkanDevice->flushCommandBuffer(cmdBuffer, queue);
}

/*
Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void TutorialRayTracing::createBottomLevelAccelerationStructure()
{
   if (!_device)
   {
      _device = new genesis::Device(VulkanExampleBase::device, VulkanExampleBase::queue, VulkanExampleBase::cmdPool, VulkanExampleBase::deviceMemoryProperties);
      genesis::VulkanFunctionsInitializer::initialize(_device);
   }
   _gltfModel = new genesis::VulkanGltfModel(_device, true, true);
#if SPONZA
   _gltfModel->loadFromFile(getAssetPath() + "models/sponza/sponza.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif

#if VENUS
   _gltfModel->loadFromFile(getAssetPath() + "models/venus.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif

   _gltfModel->buildBlas();

   // Setup identity transform matrix
   VkTransformMatrixKHR transformMatrix = {
   1.0f, 0.0f, 0.0f, 0.0f,
   0.0f, 1.0f, 0.0f, 0.0f,
   0.0f, 0.0f, 1.0f, 0.0f
   };

   VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &transformBuffer,
      sizeof(VkTransformMatrixKHR),
      &transformMatrix));

   VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

   transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer);
}

/*
The top level acceleration structure contains the scene's object instances
*/
void TutorialRayTracing::createTopLevelAccelerationStructure()
{
   VkTransformMatrixKHR transformMatrix = {
   1.0f, 0.0f, 0.0f, 0.0f,
   0.0f, 1.0f, 0.0f, 0.0f,
   0.0f, 0.0f, 1.0f, 0.0f };

   VkAccelerationStructureInstanceKHR instance{};
   instance.transform = transformMatrix;
   instance.instanceCustomIndex = 0;
   instance.mask = 0xFF;
   instance.instanceShaderBindingTableRecordOffset = 0;
   instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
   instance.accelerationStructureReference = _gltfModel->blas()->deviceAddress();

   // Buffer for instance data
   vks::Buffer instancesBuffer;
   VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &instancesBuffer,
      sizeof(VkAccelerationStructureInstanceKHR),
      &instance));

   VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
   instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

   VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
   accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
   accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
   accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
   accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
   accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
   accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

   // Get size info
   /*
   The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
   */
   VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
   accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
   accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
   accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
   accelerationStructureBuildGeometryInfo.geometryCount = 1;
   accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

   uint32_t primitive_count = 1;

   VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
   accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
   genesis::vkGetAccelerationStructureBuildSizesKHR(
      device,
      VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &accelerationStructureBuildGeometryInfo,
      &primitive_count,
      &accelerationStructureBuildSizesInfo);

   topLevelAS = new genesis::AccelerationStructure(_device, genesis::TLAS, accelerationStructureBuildSizesInfo.accelerationStructureSize);

   // Create a small scratch buffer used during build of the top level acceleration structure
   RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

   VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
   accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
   accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
   accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
   accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
   accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS->handle();
   accelerationBuildGeometryInfo.geometryCount = 1;
   accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
   accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

   VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
   accelerationStructureBuildRangeInfo.primitiveCount = 1;
   accelerationStructureBuildRangeInfo.primitiveOffset = 0;
   accelerationStructureBuildRangeInfo.firstVertex = 0;
   accelerationStructureBuildRangeInfo.transformOffset = 0;
   std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

   // Build the acceleration structure on the device via a one-time command buffer submission
   // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
   VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
   genesis::vkCmdBuildAccelerationStructuresKHR(
      commandBuffer,
      1,
      &accelerationBuildGeometryInfo,
      accelerationBuildStructureRangeInfos.data());
   vulkanDevice->flushCommandBuffer(commandBuffer, queue);

   VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
   accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
   accelerationDeviceAddressInfo.accelerationStructure = topLevelAS->handle();

   deleteScratchBuffer(scratchBuffer);
   instancesBuffer.destroy();
}

/*
Create the Shader Binding Tables that binds the programs and top-level acceleration structure

SBT Layout used in this sample:

/-----------\
| raygen    |
|-----------|
| miss      |
|-----------|
| hit       |
\-----------/

*/
void TutorialRayTracing::createShaderBindingTable() {
   const uint32_t handleSize = _rayTracingPipelineProperties.shaderGroupHandleSize;
   const uint32_t handleSizeAligned = vks::tools::alignedSize(_rayTracingPipelineProperties.shaderGroupHandleSize, _rayTracingPipelineProperties.shaderGroupHandleAlignment);
   const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
   const uint32_t sbtSize = groupCount * handleSizeAligned;

   std::vector<uint8_t> shaderHandleStorage(sbtSize);
   VK_CHECK_RESULT(genesis::vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

   const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
   const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &raygenShaderBindingTable, handleSize));
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &missShaderBindingTable, handleSize));
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &hitShaderBindingTable, handleSize));

   // Copy handles
   raygenShaderBindingTable.map();
   missShaderBindingTable.map();
   hitShaderBindingTable.map();
   memcpy(raygenShaderBindingTable.mapped, shaderHandleStorage.data(), handleSize);
   memcpy(missShaderBindingTable.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
   memcpy(hitShaderBindingTable.mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
}

/*
Create the descriptor sets used for the ray tracing dispatch
*/
void TutorialRayTracing::createDescriptorSets()
{
   std::vector<VkDescriptorPoolSize> poolSizes = {
   { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
   { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
   { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
   };
   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::vulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::vulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
   VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

   VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
   descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
   descriptorAccelerationStructureInfo.pAccelerationStructures = &(topLevelAS->handle());

   int bindingIndex = 0;

   VkWriteDescriptorSet accelerationStructureWrite{};
   accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   // The specialized acceleration structure descriptor has to be chained
   accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
   accelerationStructureWrite.dstSet = descriptorSet;
   accelerationStructureWrite.dstBinding = bindingIndex++;
   accelerationStructureWrite.descriptorCount = 1;
   accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

   VkDescriptorImageInfo storageImageDescriptor{};
   storageImageDescriptor.imageView = storageImage.view;
   storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

   std::vector<VkWriteDescriptorSet> writeDescriptorSets = { 
        accelerationStructureWrite
      , genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &storageImageDescriptor)
      , genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++, _sceneUbo->descriptorPtr())
      , genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, _gltfModel->vertexBuffer()->descriptorPtr())
      , genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, _gltfModel->indexBuffer()->descriptorPtr())
   };

   vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

/*
Create our ray tracing pipeline
*/
void TutorialRayTracing::createRayTracingPipeline()
{
   int bindingIndex = 0;

   std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
      genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, bindingIndex++)
   ,  genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, bindingIndex++)
   ,  genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   ,  genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   ,  genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   };

   VkDescriptorSetLayoutCreateInfo descriptorSetlayoutInfo = genesis::vulkanInitializers::descriptorSetLayoutCreateInfo(descriptorSetLayoutBindings);
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetlayoutInfo, nullptr, &descriptorSetLayout));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { descriptorSetLayout, _gltfModel->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::vulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());
   VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

   /*
   Setup ray tracing shader groups
   */
   std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

   // Ray generation group
   {
      shaderStages.push_back(loadShader(getShadersPath() + "tutorial_raytracing/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
      VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
      shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
      shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
      shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
      shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
      shaderGroups.push_back(shaderGroup);
   }

   // Miss group
   {
      shaderStages.push_back(loadShader(getShadersPath() + "tutorial_raytracing/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
      VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
      shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
      shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
      shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
      shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
      shaderGroups.push_back(shaderGroup);
   }

   // Closest hit group
   {
      shaderStages.push_back(loadShader(getShadersPath() + "tutorial_raytracing/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
      VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
      shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
      shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
      shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
      shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
      shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
      shaderGroups.push_back(shaderGroup);
   }

   /*
   Create the ray tracing pipeline
   */
   VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
   rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
   rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
   rayTracingPipelineCI.pStages = shaderStages.data();
   rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
   rayTracingPipelineCI.pGroups = shaderGroups.data();
   rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
   rayTracingPipelineCI.layout = pipelineLayout;
   VK_CHECK_RESULT(genesis::vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
}

/*
Create the uniform buffer used to pass matrices to the ray tracing ray generation shader
*/

struct SceneUbo
{
   glm::mat4 viewMatrix;
   glm::mat4 viewInverse;
   glm::mat4 projInverse;
   int vertexSizeInBytes;
};

void TutorialRayTracing::updateSceneUbo()
{
   SceneUbo ubo;
   ubo.viewMatrix = camera.matrices.view;
   ubo.viewInverse = glm::inverse(camera.matrices.view);
   ubo.projInverse = glm::inverse(camera.matrices.perspective);
   ubo.vertexSizeInBytes = sizeof(genesis::Vertex);

   uint8_t* data = (uint8_t*)_sceneUbo->stagingBuffer();
   int size = sizeof(SceneUbo);
   memcpy(data, &ubo, sizeof(SceneUbo));
   _sceneUbo->syncToGpu(false);
}

void TutorialRayTracing::createSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

/*
If the window has been resized, we need to recreate the storage image and it's descriptor
*/
void TutorialRayTracing::handleResize()
{
   // Delete allocated resources
   vkDestroyImageView(device, storageImage.view, nullptr);
   vkDestroyImage(device, storageImage.image, nullptr);
   vkFreeMemory(device, storageImage.memory, nullptr);
   // Recreate image
   createStorageImage();
   // Update descriptor
   VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
   VkWriteDescriptorSet resultImageWrite = genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
   vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
}

/*
Command buffer generation
*/
void TutorialRayTracing::buildCommandBuffers()
{
   if (resized)
   {
      handleResize();
   }

   VkCommandBufferBeginInfo cmdBufInfo = genesis::vulkanInitializers::commandBufferBeginInfo();

   VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

   for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
   {
      VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

      /*
      Setup the buffer regions pointing to the shaders in our shader binding table
      */

      const uint32_t handleSizeAligned = vks::tools::alignedSize(_rayTracingPipelineProperties.shaderGroupHandleSize, _rayTracingPipelineProperties.shaderGroupHandleAlignment);

      VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
      raygenShaderSbtEntry.deviceAddress = getBufferDeviceAddress(raygenShaderBindingTable.buffer);
      raygenShaderSbtEntry.stride = handleSizeAligned;
      raygenShaderSbtEntry.size = handleSizeAligned;

      VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
      missShaderSbtEntry.deviceAddress = getBufferDeviceAddress(missShaderBindingTable.buffer);
      missShaderSbtEntry.stride = handleSizeAligned;
      missShaderSbtEntry.size = handleSizeAligned;

      VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
      hitShaderSbtEntry.deviceAddress = getBufferDeviceAddress(hitShaderBindingTable.buffer);
      hitShaderSbtEntry.stride = handleSizeAligned;
      hitShaderSbtEntry.size = handleSizeAligned;

      VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

      /*
      Dispatch the ray tracing commands
      */
      vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

      std::uint32_t firstSet = 1;
      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, firstSet, std::uint32_t(_gltfModel->descriptorSets().size()), _gltfModel->descriptorSets().data(), 0, nullptr);

      genesis::vkCmdTraceRaysKHR(
         drawCmdBuffers[i],
         &raygenShaderSbtEntry,
         &missShaderSbtEntry,
         &hitShaderSbtEntry,
         &callableShaderSbtEntry,
         width,
         height,
         1);

      /*
      Copy ray tracing output to swap chain image
      */

      // Prepare current swap chain image as transfer destination
      vks::tools::setImageLayout(
         drawCmdBuffers[i],
         swapChain.images[i],
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         subresourceRange);

      // Prepare ray tracing output image as transfer source
      vks::tools::setImageLayout(
         drawCmdBuffers[i],
         storageImage.image,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         subresourceRange);

      VkImageCopy copyRegion{};
      copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
      copyRegion.srcOffset = { 0, 0, 0 };
      copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
      copyRegion.dstOffset = { 0, 0, 0 };
      copyRegion.extent = { width, height, 1 };
      vkCmdCopyImage(drawCmdBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

      // Transition swap chain image back for presentation
      vks::tools::setImageLayout(
         drawCmdBuffers[i],
         swapChain.images[i],
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         subresourceRange);

      // Transition ray tracing output image back to general layout
      vks::tools::setImageLayout(
         drawCmdBuffers[i],
         storageImage.image,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         subresourceRange);

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
   }
}



void TutorialRayTracing::getEnabledFeatures()
{
   // Enable features required for ray tracing using feature chaining via pNext		
   _enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
   _enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

   _enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
   _enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
   _enabledRayTracingPipelineFeatures.pNext = &_enabledBufferDeviceAddresFeatures;

   _enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   _enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
   _enabledAccelerationStructureFeatures.pNext = &_enabledRayTracingPipelineFeatures;

   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.pNext = &_enabledAccelerationStructureFeatures;

   deviceCreatepNextChain = &_physicalDeviceDescriptorIndexingFeatures;
}

void TutorialRayTracing::prepare()
{
   VulkanExampleBase::prepare();


   // Get ray tracing pipeline properties, which will be used later on in the sample
   _rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
   VkPhysicalDeviceProperties2 deviceProperties2{};
   deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   deviceProperties2.pNext = &_rayTracingPipelineProperties;
   vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

   // Get acceleration structure properties, which will be used later on in the sample
   _accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   VkPhysicalDeviceFeatures2 deviceFeatures2{};
   deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   deviceFeatures2.pNext = &_accelerationStructureFeatures;
   vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

   // Create the acceleration structures used to render the ray traced scene
   createBottomLevelAccelerationStructure();
   createTopLevelAccelerationStructure();

   createStorageImage();
   createSceneUbo();
   createRayTracingPipeline();
   createShaderBindingTable();
   createDescriptorSets();
   buildCommandBuffers();
   prepared = true;
}

void TutorialRayTracing::draw()
{
   VulkanExampleBase::prepareFrame();
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
   VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
   VulkanExampleBase::submitFrame();
}

void TutorialRayTracing::render()
{
   if (!prepared)
      return;
   draw();
   if (camera.updated)
      updateSceneUbo();
}

