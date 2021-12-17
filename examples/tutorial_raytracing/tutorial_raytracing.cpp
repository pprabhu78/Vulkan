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
#include "StorageImage.h"
#include "ImageTransitions.h"
#include "RenderPass.h"
#include "ScreenShotUtility.h"
#include "VulkanInitializers.h"

#include <chrono>

#define VENUS 0
#define SPONZA 0
#define CORNELL 1

void TutorialRayTracing::resetCamera()
{
#if VENUS
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -8.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
#endif

#if CORNELL
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -14.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
   _pushConstants.contributionFromEnvironment = 0.01f;
#endif

#if SPONZA
   camera.type = Camera::CameraType::firstperson;
   camera.rotationSpeed = 0.2f;
   camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
   camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
   _pushConstants.contributionFromEnvironment = 10;
#endif
}

TutorialRayTracing::TutorialRayTracing()
   : _gltfModel(nullptr)
   , _pushConstants{}
{
   _pushConstants.frameIndex = -1;
   title = "genesis: path tracer";
   settings.overlay = false;
   camera.type = Camera::CameraType::lookat;
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
   camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
   camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.5f));

   resetCamera();

   // Require Vulkan 1.2
   apiVersion = VK_API_VERSION_1_2;

   // Ray tracing related extensions required by this sample
   enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

   // Required by VK_KHR_acceleration_structure
   enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

   // Required for VK_KHR_ray_tracing_pipeline
   enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

   // Required by VK_KHR_spirv_1_4
   enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

   // For descriptor indexing
   enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

   enabledDeviceExtensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);

   enabledFeatures.shaderInt64 = true;
}

TutorialRayTracing::~TutorialRayTracing()
{
   vkDestroyPipeline(device, _rayTracingPipeline, nullptr);
   vkDestroyPipelineLayout(device, _rayTracingPipelineLayout, nullptr);
   vkDestroyDescriptorSetLayout(device, _rayTracingDescriptorSetLayout, nullptr);

   deleteStorageImages();

   delete topLevelAS;

   transformBuffer.destroy();
   
   _raygenShaderBindingTable.destroy();
   _missShaderBindingTable.destroy();
   _hitShaderBindingTable.destroy();
   
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

void TutorialRayTracing::writeStorageImageDescriptors()
{
   VkDescriptorImageInfo intermediateImageDescriptor{ VK_NULL_HANDLE, _intermediateImage->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL };
   VkDescriptorImageInfo finalImageDescriptor{ VK_NULL_HANDLE, _finalImageToPresent->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL };

   int bindingIndex = 1;
   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
     genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &intermediateImageDescriptor)
   , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &finalImageDescriptor)
   };

   vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void TutorialRayTracing::deleteStorageImages()
{
   delete _finalImageToPresent;
   _finalImageToPresent = nullptr;

   delete _intermediateImage;
   _intermediateImage = nullptr;
}

/*
Set up a storage image that the ray generation shader will be writing to
*/
void TutorialRayTracing::createStorageImages()
{
   // intermediate image does computations in full floating point
   _intermediateImage   = new genesis::StorageImage(_device, VK_FORMAT_R32G32B32A32_SFLOAT, width, height
      , VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL);

   // final image is used for presentation. So, its the same format as the swap chain
   _finalImageToPresent = new genesis::StorageImage(_device, swapChain.colorFormat, width, height
      , VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL);

   genesis::ImageTransitions transitions;
   VkCommandBuffer commandBuffer = _device->getCommandBuffer(true);

   transitions.setImageLayout(commandBuffer, _intermediateImage->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
   transitions.setImageLayout(commandBuffer, _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

   _device->flushCommandBuffer(commandBuffer);
}

/*
Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void TutorialRayTracing::createBottomLevelAccelerationStructure()
{

   _gltfModel = new genesis::VulkanGltfModel(_device, true, true);
#if SPONZA
   _gltfModel->loadFromFile(getAssetPath() + "models/sponza/sponza.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif

#if VENUS
   _gltfModel->loadFromFile(getAssetPath() + "models/venus.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif

#if CORNELL
   _gltfModel->loadFromFile(getAssetPath() + "models/cornellBox.gltf"
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
   VK_CHECK_RESULT(genesis::vkGetRayTracingShaderGroupHandlesKHR(device, _rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

   const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
   const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &_raygenShaderBindingTable, handleSize));
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &_missShaderBindingTable, handleSize));
   VK_CHECK_RESULT(vulkanDevice->createBuffer(bufferUsageFlags, memoryUsageFlags, &_hitShaderBindingTable, handleSize));

   // Copy handles
   _raygenShaderBindingTable.map();
   _missShaderBindingTable.map();
   _hitShaderBindingTable.map();
   memcpy(_raygenShaderBindingTable.mapped, shaderHandleStorage.data(), handleSize);
   memcpy(_missShaderBindingTable.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
   memcpy(_hitShaderBindingTable.mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);

   _raygenShaderSbtEntry.deviceAddress = getBufferDeviceAddress(_raygenShaderBindingTable.buffer);
   _raygenShaderSbtEntry.stride = handleSizeAligned;
   _raygenShaderSbtEntry.size = handleSizeAligned;

   _missShaderSbtEntry.deviceAddress = getBufferDeviceAddress(_missShaderBindingTable.buffer);
   _missShaderSbtEntry.stride = handleSizeAligned;
   _missShaderSbtEntry.size = handleSizeAligned;

   _hitShaderSbtEntry.deviceAddress = getBufferDeviceAddress(_hitShaderBindingTable.buffer);
   _hitShaderSbtEntry.stride = handleSizeAligned;
   _hitShaderSbtEntry.size = handleSizeAligned;

   VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};
}

/*
Create the descriptor sets used for the ray tracing dispatch
*/
void TutorialRayTracing::createDescriptorSets()
{
   std::vector<VkDescriptorPoolSize> poolSizes = {
   { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
   { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
   { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
   };
   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &_rayTracingDescriptorSetLayout, 1);
   VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &_rayTracingDescriptorSet));

   VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = genesis::VulkanInitializers::writeDescriptorSetAccelerationStructureKHR();
   descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
   descriptorAccelerationStructureInfo.pAccelerationStructures = &(topLevelAS->handle());

   int bindingIndex = 0;

   VkWriteDescriptorSet accelerationStructureWrite{};
   accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   // The specialized acceleration structure descriptor has to be chained
   accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
   accelerationStructureWrite.dstSet = _rayTracingDescriptorSet;
   accelerationStructureWrite.dstBinding = bindingIndex++;
   accelerationStructureWrite.descriptorCount = 1;
   accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

   VkDescriptorImageInfo intermediateImageDescriptor{ VK_NULL_HANDLE, _intermediateImage->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL };
   VkDescriptorImageInfo finalImageDescriptor{ VK_NULL_HANDLE, _finalImageToPresent->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL };

   std::vector<VkWriteDescriptorSet> writeDescriptorSets = { 
        accelerationStructureWrite
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &intermediateImageDescriptor)
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &finalImageDescriptor)
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++, _sceneUbo->descriptorPtr())
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, _gltfModel->vertexBuffer()->descriptorPtr())
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++, _gltfModel->indexBuffer()->descriptorPtr())
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
      genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
   };

   VkDescriptorSetLayoutCreateInfo descriptorSetlayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(descriptorSetLayoutBindings);
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetlayoutInfo, nullptr, &_rayTracingDescriptorSetLayout));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _rayTracingDescriptorSetLayout, _gltfModel->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::VulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   // Push constant: we want to be able to update constants used by the shaders
   VkPushConstantRange pushConstant{ VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                                    0, sizeof(PushConstants) };

   pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
   pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

   VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &_rayTracingPipelineLayout));

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
   rayTracingPipelineCI.layout = _rayTracingPipelineLayout;
   VK_CHECK_RESULT(genesis::vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &_rayTracingPipeline));
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
void TutorialRayTracing::windowResized()
{
   // Delete allocated resources
   deleteStorageImages();
   // Recreate image
   createStorageImages();
   // Update descriptor
   writeStorageImageDescriptors();
   
   _pushConstants.frameIndex = -1;
}

void TutorialRayTracing::saveScreenShot(void)
{
   using namespace std;
   using namespace std::chrono;

   time_t tt = system_clock::to_time_t(system_clock::now());
   tm utc_tm = *gmtime(&tt);
   tm local_tm = *localtime(&tt);

   std::stringstream ss;
   ss<< local_tm.tm_year + 1900 << '-';
   ss<< local_tm.tm_mon + 1 << '-';
   ss<< local_tm.tm_mday << '_';
   ss << local_tm.tm_hour;
   ss << local_tm.tm_min;
   if (local_tm.tm_sec < 10)
   {
      ss << "0";
   }
   ss << local_tm.tm_sec;

   std::string fileName = ss.str();
   
   genesis::ScreenShotUtility screenShotUtility(_device);
   screenShotUtility.takeScreenShot("..\\screenshots\\" + fileName + ".png"
      , swapChain.images[currentBuffer], swapChain.colorFormat
      , width, height);
}

void TutorialRayTracing::keyPressed(uint32_t key)
{
   if (key == KEY_F5)
   {
      saveScreenShot();
   }
   else if (key == KEY_SPACE)
   {
      resetCamera();
   }
   else if (key == KEY_F4)
   {
      settings.overlay = !settings.overlay;
   }
}

/*
Command buffer generation
*/
void TutorialRayTracing::rayTrace(int commandBufferIndex)
{
   VkCommandBufferBeginInfo cmdBufInfo = genesis::VulkanInitializers::commandBufferBeginInfo();

   VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[commandBufferIndex], &cmdBufInfo));

   vkCmdBindPipeline(drawCmdBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipeline);
   vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipelineLayout, 0, 1, &_rayTracingDescriptorSet, 0, 0);

   std::uint32_t firstSet = 1;
   vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipelineLayout, firstSet, std::uint32_t(_gltfModel->descriptorSets().size()), _gltfModel->descriptorSets().data(), 0, nullptr);

   ++_pushConstants.frameIndex;
   _pushConstants.clearColor = genesis::Vector4_32(1, 1, 1, 1);
   vkCmdPushConstants(
      drawCmdBuffers[commandBufferIndex],
      _rayTracingPipelineLayout,
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
      0,
      sizeof(PushConstants),
      &_pushConstants);

   genesis::vkCmdTraceRaysKHR(
      drawCmdBuffers[commandBufferIndex],
      &_raygenShaderSbtEntry,
      &_missShaderSbtEntry,
      &_hitShaderSbtEntry,
      &_callableShaderSbtEntry,
      width,
      height,
      1);

   genesis::ImageTransitions transitions;
   // Prepare current swap chain image as transfer destination
   VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
   transitions.setImageLayout(drawCmdBuffers[commandBufferIndex],swapChain.images[commandBufferIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

   // Prepare ray tracing output image as transfer source
   transitions.setImageLayout(drawCmdBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

   VkImageCopy copyRegion{};
   copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
   copyRegion.srcOffset = { 0, 0, 0 };
   copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
   copyRegion.dstOffset = { 0, 0, 0 };
   copyRegion.extent = { width, height, 1 };
   vkCmdCopyImage(drawCmdBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[commandBufferIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

   // Transition swap chain image back for presentation
   transitions.setImageLayout(drawCmdBuffers[commandBufferIndex], swapChain.images[commandBufferIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresourceRange);

   // Transition ray tracing output image back to general layout
   transitions.setImageLayout(drawCmdBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, subresourceRange);

   drawImgui(drawCmdBuffers[commandBufferIndex], frameBuffers[commandBufferIndex]);

   VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[commandBufferIndex]));
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

   _physicalDeviceShaderClockFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
   _physicalDeviceShaderClockFeaturesKHR.shaderDeviceClock = true;
   _physicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock = true;
   _physicalDeviceShaderClockFeaturesKHR.pNext = &_physicalDeviceDescriptorIndexingFeatures;
   
   deviceCreatepNextChain = &_physicalDeviceShaderClockFeaturesKHR;
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

   createStorageImages();
   createSceneUbo();
   createRayTracingPipeline();
   createShaderBindingTable();
   createDescriptorSets();
   prepared = true;
}

void TutorialRayTracing::draw()
{
   VulkanExampleBase::prepareFrame();

   rayTrace(currentBuffer);

   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
   VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
   VulkanExampleBase::submitFrame();
}

void TutorialRayTracing::render()
{
   if (!prepared)
   {
      return;
   }
   draw();

   if (camera.updated)
   {
      _pushConstants.frameIndex = -1;
      updateSceneUbo();
   }
}

void TutorialRayTracing::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
   if (overlay->header("Settings")) {
      if (overlay->sliderFloat("LOD bias", &_pushConstants.textureLodBias, 0.0f, 1.0f)) \
      {
         _pushConstants.frameIndex = -1;
      }
   }
}

void TutorialRayTracing::setupRenderPass()
{
   if (!_device)
   {
      _device = new genesis::Device(VulkanExampleBase::physicalDevice, VulkanExampleBase::device, VulkanExampleBase::queue, VulkanExampleBase::cmdPool);
      genesis::VulkanFunctionsInitializer::initialize(_device);
   }

   _renderPass = new genesis::RenderPass(_device, swapChain.colorFormat, depthFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
}

void TutorialRayTracing::drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
{
   VkClearValue clearValues[2];
   clearValues[0].color = defaultClearColor;
   clearValues[1].depthStencil = { 1.0f, 0 };

   VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
   renderPassBeginInfo.renderPass = _renderPass->vulkanRenderPass();
   renderPassBeginInfo.renderArea.offset = { 0, 0 };
   renderPassBeginInfo.renderArea.extent = { width, height };
   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;
   renderPassBeginInfo.framebuffer = framebuffer;

   vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
   VulkanExampleBase::drawUI(commandBuffer);
   vkCmdEndRenderPass(commandBuffer);
}
