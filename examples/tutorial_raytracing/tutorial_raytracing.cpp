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
#include "VulkanGltf.h"
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "StorageImage.h"
#include "ImageTransitions.h"
#include "RenderPass.h"
#include "ScreenShotUtility.h"
#include "VulkanInitializers.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "Texture.h"
#include "VulkanFunctions.h"

#include "Cell.h"
#include "CellManager.h"
#include "Tlas.h"
#include "IndirectLayout.h"

#include <chrono>
#include <sstream>

//#define VENUS 1
#define SPONZA 1
//#define CORNELL 1
//#define SPHERE

//#define SKYBOX_PISA 1
#define SKYBOX_YOKOHOMA 1

using namespace genesis;
using namespace genesis::tools;

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

#if (defined SPHERE)
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
#endif

#if (defined SPONZA)
   camera.type = genesis::Camera::CameraType::firstperson;
   camera.rotationSpeed = 0.2f;
   camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
   camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
   _pushConstants.contributionFromEnvironment = 10;
#endif
}

TutorialRayTracing::TutorialRayTracing()
   : _pushConstants{}
{
   _pushConstants.frameIndex = -1;
   title = "genesis: path tracer";
   settings.overlay = false;
   camera.type = genesis::Camera::CameraType::lookat;
   camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
   camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
   camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.5f));

   resetCamera();

   // Require Vulkan 1.2
   apiVersion = VK_API_VERSION_1_2;

   // Ray tracing related extensions required by this sample
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

   // Required by VK_KHR_acceleration_structure
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

   // Required for VK_KHR_ray_tracing_pipeline
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

   // Required by VK_KHR_spirv_1_4
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

   // For descriptor indexing
   _enabledPhysicalDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
}

void TutorialRayTracing::enableFeatures()
{
   _physicalDevice->enabledPhysicalDeviceFeatures().shaderInt64 = true;

   // Enable features required for ray tracing using feature chaining via pNext		
   _enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
   _enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

   _enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
   _enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
   _enabledRayTracingPipelineFeatures.pNext = &_enabledBufferDeviceAddressFeatures;

   _enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   _enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
   _enabledAccelerationStructureFeatures.pNext = &_enabledRayTracingPipelineFeatures;

   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.pNext = &_enabledAccelerationStructureFeatures;

   _physicalDeviceShaderClockFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
   _physicalDeviceShaderClockFeaturesKHR.shaderDeviceClock = true;
   _physicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock = true;
   _physicalDeviceShaderClockFeaturesKHR.pNext = &_physicalDeviceDescriptorIndexingFeatures;
   
   deviceCreatepNextChain = &_physicalDeviceShaderClockFeaturesKHR;
}

TutorialRayTracing::~TutorialRayTracing()
{
   vkDestroyPipeline(_device->vulkanDevice(), _rayTracingPipeline, nullptr);
   vkDestroyPipelineLayout(_device->vulkanDevice(), _rayTracingPipelineLayout, nullptr);
   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _rayTracingDescriptorSetLayout, nullptr);

   deleteStorageImages();

   delete _cellManager;
   
   delete _raygenShaderBindingTable;
   delete _missShaderBindingTable;
   delete _hitShaderBindingTable;
   
   delete _sceneUbo;

   delete _skyCubeMapTexture;
   delete _skyCubeMapImage;
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

   vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
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
   VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

   transitions.setImageLayout(commandBuffer, _intermediateImage->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
   transitions.setImageLayout(commandBuffer, _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

   _device->flushCommandBuffer(commandBuffer);
}

/*
Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void TutorialRayTracing::createBottomLevelAccelerationStructure()
{
   const uint32_t glTFLoadingFlags = genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors;



   _skyCubeMapImage = new genesis::Image(_device);
#if (defined SKYBOX_YOKOHOMA)
   _pushConstants.environmentMapCoordTransform.x = -1;
   _pushConstants.environmentMapCoordTransform.y = -1;
   _skyCubeMapImage->loadFromFileCubeMap(getAssetsPath() + "textures/cubemap_yokohama_rgba.ktx");
#endif

#if (defined SKYBOX_PISA)
   _skyCubeMapImage->loadFromFileCubeMap(getAssetsPath() + "textures/hdr/pisa_cube.ktx");
#endif
   _skyCubeMapTexture = new genesis::Texture(_skyCubeMapImage);

}

/*
The top level acceleration structure contains the scene's object instances
*/
void TutorialRayTracing::createTopLevelAccelerationStructure()
{
   std::string gltfModel;
   std::string gltfModel2;

#if (defined SPONZA)
   gltfModel = getAssetsPath() + "models/sponza/sponza.gltf";
#endif

#if defined VENUS
   gltfModel = getAssetsPath() + "models/venus.gltf";
#endif

#if defined CORNELL
   gltfModel = getAssetsPath() + "models/cornellBox.gltf";
#endif

#if defined SPHERE
   gltfModel = getAssetsPath() + "models/sphere.gltf";
#endif

   const uint32_t glTFLoadingFlags = genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors;
   _cellManager = new genesis::CellManager(_device, glTFLoadingFlags);

   _cellManager->addInstance(gltfModel, mat4());

#if 1
   gltfModel2 = getAssetsPath() + "../../glTF-Sample-Models/2.0//WaterBottle//glTF/WaterBottle.gltf";

   _cellManager->addInstance(gltfModel2, glm::translate(glm::mat4(), glm::vec3(-2, -1.0f, 0.0f)));
   _cellManager->addInstance(gltfModel2, glm::translate(glm::mat4(), glm::vec3(-3, -2.0f, 0.0f)));
#endif

   _cellManager->buildTlases();
   _cellManager->buildDrawBuffers();
   _cellManager->buildLayouts();
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
   const uint32_t handleSizeAligned = genesis::tools::alignedSize(_rayTracingPipelineProperties.shaderGroupHandleSize, _rayTracingPipelineProperties.shaderGroupHandleAlignment);
   const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
   const uint32_t sbtSize = groupCount * handleSizeAligned;

   std::vector<uint8_t> shaderHandleStorage(sbtSize);
   VK_CHECK_RESULT(genesis::vkGetRayTracingShaderGroupHandlesKHR(_device->vulkanDevice(), _rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

   const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
   const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

   _raygenShaderBindingTable = new genesis::VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);
   _missShaderBindingTable = new genesis::VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);
   _hitShaderBindingTable = new genesis::VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);

   // Copy handles
   _raygenShaderBindingTable->map();
   _missShaderBindingTable->map();
   _hitShaderBindingTable->map();
   memcpy(_raygenShaderBindingTable->_mapped, shaderHandleStorage.data(), handleSize);
   memcpy(_missShaderBindingTable->_mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
   memcpy(_hitShaderBindingTable->_mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);

   _raygenShaderSbtEntry.deviceAddress = _raygenShaderBindingTable->deviceAddress();
   _raygenShaderSbtEntry.stride = handleSizeAligned;
   _raygenShaderSbtEntry.size = handleSizeAligned;

   _missShaderSbtEntry.deviceAddress = _missShaderBindingTable->deviceAddress();
   _missShaderSbtEntry.stride = handleSizeAligned;
   _missShaderSbtEntry.size = handleSizeAligned;

   _hitShaderSbtEntry.deviceAddress = _hitShaderBindingTable->deviceAddress();
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
   { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
   };
   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &_rayTracingDescriptorSetLayout, 1);
   VK_CHECK_RESULT(vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &_rayTracingDescriptorSet));

   VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = genesis::VulkanInitializers::writeDescriptorSetAccelerationStructureKHR();
   descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
   descriptorAccelerationStructureInfo.pAccelerationStructures = &(_cellManager->cell(0)->tlas()->handle());

   VkDescriptorImageInfo intermediateImageDescriptor = genesis::VulkanInitializers::descriptorImageInfo(VK_NULL_HANDLE, _intermediateImage->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL);
   VkDescriptorImageInfo finalImageDescriptor = genesis::VulkanInitializers::descriptorImageInfo(VK_NULL_HANDLE, _finalImageToPresent->vulkanImageView(), VK_IMAGE_LAYOUT_GENERAL);

   int bindingIndex = 0;
   std::vector<VkWriteDescriptorSet> writeDescriptorSets = { 
        genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, bindingIndex++, &descriptorAccelerationStructureInfo)
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &intermediateImageDescriptor)
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindingIndex++, &finalImageDescriptor)
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++, _sceneUbo->descriptorPtr())
      , genesis::VulkanInitializers::writeDescriptorSet(_rayTracingDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindingIndex++, _skyCubeMapTexture->descriptorPtr())
   };

   vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
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
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, bindingIndex++)
   };

   VkDescriptorSetLayoutCreateInfo descriptorSetlayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(descriptorSetLayoutBindings);
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &descriptorSetlayoutInfo, nullptr, &_rayTracingDescriptorSetLayout));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _rayTracingDescriptorSetLayout, _cellManager->cell(0)->layout()->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::VulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   // Push constant: we want to be able to update constants used by the shaders
   VkPushConstantRange pushConstant{ VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                                    0, sizeof(PushConstants) };

   pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
   pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

   VK_CHECK_RESULT(vkCreatePipelineLayout(_device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_rayTracingPipelineLayout));

   /*
   Setup ray tracing shader groups
   */
   std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

   // Ray generation group
   {
      Shader* shader = loadShader(getShadersPath() + "tutorial_raytracing/raygen.rgen.spv", genesis::ST_RT_RAYGEN);
      shaderStages.push_back(shader->pipelineShaderStageCreateInfo());
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
      Shader* shader = loadShader(getShadersPath() + "tutorial_raytracing/miss.rmiss.spv", genesis::ST_RT_MISS);
      shaderStages.push_back(shader->pipelineShaderStageCreateInfo());

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
      Shader* shader = loadShader(getShadersPath() + "tutorial_raytracing/closesthit.rchit.spv", genesis::ST_RT_CLOSEST_HIT);
      shaderStages.push_back(shader->pipelineShaderStageCreateInfo());

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
   VK_CHECK_RESULT(genesis::vkCreateRayTracingPipelinesKHR(_device->vulkanDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &_rayTracingPipeline));
}

/*
Create the uniform buffer used to pass matrices to the ray tracing ray generation shader
*/

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
   vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipelineLayout, firstSet, std::uint32_t(_cellManager->cell(0)->layout()->descriptorSets().size()), _cellManager->cell(0)->layout()->descriptorSets().data(), 0, nullptr);

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

void TutorialRayTracing::prepare()
{
   VulkanExampleBase::prepare();

   // Get ray tracing pipeline properties, which will be used later on in the sample
   _rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
   VkPhysicalDeviceProperties2 deviceProperties2{};
   deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   deviceProperties2.pNext = &_rayTracingPipelineProperties;
   vkGetPhysicalDeviceProperties2(_physicalDevice->vulkanPhysicalDevice(), &deviceProperties2);

   // Get acceleration structure properties, which will be used later on in the sample
   _accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   VkPhysicalDeviceFeatures2 deviceFeatures2{};
   deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   deviceFeatures2.pNext = &_accelerationStructureFeatures;
   vkGetPhysicalDeviceFeatures2(_physicalDevice->vulkanPhysicalDevice(), &deviceFeatures2);

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
   VK_CHECK_RESULT(vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
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

void TutorialRayTracing::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
{
   if (overlay->header("Settings")) {
      if (overlay->sliderFloat("LOD bias", &_pushConstants.textureLodBias, 0.0f, 1.0f)) \
      {
         _pushConstants.frameIndex = -1;
      }
      if (overlay->sliderFloat("reflectivity", &_pushConstants.reflectivity, 0, 1)) {
      }
   }
}

void TutorialRayTracing::setupRenderPass()
{
   _renderPass = new genesis::RenderPass(_device, swapChain.colorFormat, _depthFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
}

void TutorialRayTracing::drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
{
   VkClearValue clearValues[2];
   clearValues[0].color = defaultClearColor;
   clearValues[1].depthStencil = { 1.0f, 0 };

   VkRenderPassBeginInfo renderPassBeginInfo = genesis::VulkanInitializers::renderPassBeginInfo();
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
