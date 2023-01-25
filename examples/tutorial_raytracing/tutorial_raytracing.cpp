/*
* Ray tracing sample
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "tutorial_raytracing.h"

#include "Device.h"
#include "PhysicalDevice.h"
#include "Texture.h"
#include "Buffer.h"
#include "Shader.h"
#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "RenderPass.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"
#include "VulkanExtensions.h"
#include "AccelerationStructure.h"
#include "StorageImage.h"
#include "ImageTransitions.h"
#include "ScreenShotUtility.h"
#include "ShaderBindingTable.h"
#include "Tlas.h"
#include "Cell.h"
#include "CellManager.h"
#include "IndirectLayout.h"

#include <chrono>
#include <sstream>

//#define SKYBOX_PISA 1
#define SKYBOX_YOKOHOMA 1

using namespace genesis;
using namespace genesis::tools;

void TutorialRayTracing::resetCamera()
{
   if (_mainModel == "venus")
   {
      _camera.type = Camera::CameraType::lookat;
      _camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
      _camera.setRotation(glm::vec3(0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 1.0f, 256.0f);
      _pushConstants.contributionFromEnvironment = 1;
   }
   else if (_mainModel == "cornell")
   {
      _camera.type = Camera::CameraType::lookat;
      _camera.setPosition(glm::vec3(0.0f, 0.0f, -14.5f));
      _camera.setRotation(glm::vec3(0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 1.0f, 256.0f);
      _pushConstants.contributionFromEnvironment = 0;
   }
   else if (_mainModel == "sphere")
   {
      _camera.type = Camera::CameraType::lookat;
      _camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
      _camera.setRotation(glm::vec3(0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 1.0f, 256.0f);
      _pushConstants.contributionFromEnvironment = 1;
   }
   else if (_mainModel == "sponza")
   {
      _camera.type = genesis::Camera::CameraType::firstperson;
      _camera.setPosition(glm::vec3(0.0f, -1.0f, 0.0f));
      _camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 0.1f, 256.0f);

      _camera.rotationSpeed = 0.2f;
      _pushConstants.contributionFromEnvironment = 10;
   }
   else if (_mainModel == "bathroom")
   {
      _camera.type = genesis::Camera::CameraType::firstperson;

      _camera.setPosition(glm::vec3(2.42036271f, -1.83941388, -5.26105785));
      _camera.setRotation(glm::vec3(19.6f, -303.601227, 0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 0.1f, 256.0f);

      _camera.rotationSpeed = 0.2f;
      _pushConstants.contributionFromEnvironment = 1;
   }
   else
   {
      _camera.type = genesis::Camera::CameraType::lookat;
      _camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.5f));
      _camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
      _camera.setPerspective(60.0f, (float)_width / (float)_height, 0.1f, 512.0f);
   }
}

TutorialRayTracing::TutorialRayTracing()
   : _pushConstants{}
{
   _settings.overlay = false;

   for (int i = 0; i < args.size(); ++i)
   {
      const std::string arg = args[i];
      if (arg == "--autoTest")
      {
         _autoTest = true;
      }
      else if (arg == "--model" && (i + 1) < args.size())
      {
         _mainModel = args[i + 1];
      }
      else if (arg == "--dynamicRendering")
      {
         _dynamicRendering = true;
      }
   }

   if (_mainModel == "")
   {
      _mainModel = "sponza";
   }

   _title = "genesis: path tracer";

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

   // required for multi-draw
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);

   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
}

#define  ADD_FIRST(first) deviceCreatepNextChain = &first;
#define  ADD_NEXT(current, next) current.pNext = &next

void TutorialRayTracing::enableFeatures()
{
   // This is required for 64 bit math
   _physicalDevice->enabledPhysicalDeviceFeatures().shaderInt64 = true;

   // This is required for multi draw indirect
   _physicalDevice->enabledPhysicalDeviceFeatures().multiDrawIndirect = VK_TRUE;

   // Enable anisotropic filtering if supported
   if (_physicalDevice->physicalDeviceFeatures().samplerAnisotropy)
   {
      _physicalDevice->enabledPhysicalDeviceFeatures().samplerAnisotropy = VK_TRUE;
   }

   // This is required for wireframe display
   if (_physicalDevice->physicalDeviceFeatures().fillModeNonSolid)
   {
      _physicalDevice->enabledPhysicalDeviceFeatures().fillModeNonSolid = VK_TRUE;
   }

   ADD_FIRST(_enabledBufferDeviceAddressFeatures);
   _enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
   _enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

   ADD_NEXT(_enabledBufferDeviceAddressFeatures, _enabledRayTracingPipelineFeatures);
   _enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
   _enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

   ADD_NEXT(_enabledRayTracingPipelineFeatures, _enabledAccelerationStructureFeatures);
   _enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   _enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;

   ADD_NEXT(_enabledAccelerationStructureFeatures, _physicalDeviceDescriptorIndexingFeatures);
   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
   
   ADD_NEXT(_physicalDeviceDescriptorIndexingFeatures, _physicalDeviceShaderClockFeaturesKHR);
   _physicalDeviceShaderClockFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
   _physicalDeviceShaderClockFeaturesKHR.shaderDeviceClock = true;
   _physicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock = true;

   ADD_NEXT(_physicalDeviceShaderClockFeaturesKHR, _dynamicRenderingFeatures);
   _dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
   _dynamicRenderingFeatures.dynamicRendering = true;
}

void TutorialRayTracing::destroyRasterizationStuff()
{
   vkDestroyPipeline(_device->vulkanDevice(), _rasterizationPipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _rasterizationPipelineWireframe, nullptr);
   _rasterizationPipeline = 0;
   _rasterizationPipelineWireframe = 0;

   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxRasterizationPipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxRasterizationPipelineWireframe, nullptr);
   _skyBoxRasterizationPipeline = 0;
   _skyBoxRasterizationPipelineWireframe = 0;

   vkDestroyPipelineLayout(_device->vulkanDevice(), _rasterizationPipelineLayout, nullptr);
   vkDestroyPipelineLayout(_device->vulkanDevice(), _rasterizationSkyBoxPipelineLayout, nullptr);
   _rasterizationPipelineLayout = 0;
   _rasterizationSkyBoxPipelineLayout = 0;

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _rasterizationDescriptorSetLayout, nullptr);
   vkDestroyDescriptorPool(_device->vulkanDevice(), _rasterizationDescriptorPool, nullptr);
   _rasterizationDescriptorSetLayout = 0;
   _rasterizationDescriptorPool = 0;
}

void TutorialRayTracing::destroyRayTracingStuff(bool storageImages)
{
   vkDestroyPipeline(_device->vulkanDevice(), _rayTracingPipeline, nullptr);
   _rayTracingPipeline = 0;

   vkDestroyPipelineLayout(_device->vulkanDevice(), _rayTracingPipelineLayout, nullptr);
   _rayTracingPipelineLayout = 0;

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _rayTracingDescriptorSetLayout, nullptr);
   _rayTracingDescriptorSetLayout = 0;

   vkDestroyDescriptorPool(_device->vulkanDevice(), _rayTracingDescriptorPool, nullptr);
   _rayTracingDescriptorPool = 0;

   delete _shaderBindingTable;
   _shaderBindingTable = nullptr;

   if (storageImages)
   {
      deleteStorageImages();
   }
}

void TutorialRayTracing::destroyCommonStuff()
{
   delete _cellManager;

   delete _skyBoxManager;

   delete _sceneUbo;

   delete _skyCubeMapTexture;
   delete _skyCubeMapImage;
}

TutorialRayTracing::~TutorialRayTracing()
{
   destroyRayTracingStuff(true);
   destroyRasterizationStuff();
   destroyCommonStuff();
}

void TutorialRayTracing::createAndUpdateRayTracingDescriptorSets()
{
   std::vector<VkDescriptorPoolSize> poolSizes = {
   { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
   { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
   { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
   };
   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_rayTracingDescriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_rayTracingDescriptorPool, &_rayTracingDescriptorSetLayout, 1);
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

void TutorialRayTracing::createAndUpdateRasterizationDescriptorSets()
{
   std::vector<VkDescriptorPoolSize> poolSizes = {
   {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
,  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
   };

   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_rasterizationDescriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_rasterizationDescriptorPool, &_rasterizationDescriptorSetLayout, 1);
   vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &_rasterizationDescriptorSet);

   int bindingIndex = 0;
   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
   genesis::VulkanInitializers::writeDescriptorSet(_rasterizationDescriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++,&_sceneUbo->descriptor())
,  genesis::VulkanInitializers::writeDescriptorSet(_rasterizationDescriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindingIndex++,&_skyCubeMapTexture->descriptor())
   };

   vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void TutorialRayTracing::createAndUpdateDescriptorSets()
{
   createAndUpdateRayTracingDescriptorSets();
   createAndUpdateRasterizationDescriptorSets();
}

void TutorialRayTracing::reloadShaders(bool destroyExistingStuff)
{
   std::string strVulkanDir = getenv("VULKAN_SDK");
   std::string glslValidator = strVulkanDir + "\\bin\\glslangValidator.exe";

   std::string command = glslValidator + " " + "--target-env vulkan1.2 -V -o ../data/shaders/glsl/tutorial_raytracing/closesthit.rchit.spv ../data/shaders/glsl/tutorial_raytracing/closesthit.rchit";
   system(command.c_str());

   command = glslValidator + " " + "--target-env vulkan1.2 -V -o ../data/shaders/glsl/tutorial_raytracing/miss.rmiss.spv ../data/shaders/glsl/tutorial_raytracing/miss.rmiss";
   system(command.c_str());

   command = glslValidator + " " + "--target-env vulkan1.2 -V -o ../data/shaders/glsl/tutorial_raytracing/raygen.rgen.spv ../data/shaders/glsl/tutorial_raytracing/raygen.rgen";
   system(command.c_str());

   std::string glslc = strVulkanDir + "\\bin\\glslc.exe";

   command = glslc + " " + "-o ../data/shaders/glsl/tutorial_raytracing/tutorial.vert.spv ../data/shaders/glsl/tutorial_raytracing/tutorial.vert";
   system(command.c_str());

   command = glslc + " " + "-o ../data/shaders/glsl/tutorial_raytracing/tutorial.frag.spv ../data/shaders/glsl/tutorial_raytracing/tutorial.frag";
   system(command.c_str());

   command = glslc + " " + "-o ../data/shaders/glsl/tutorial_raytracing/skybox.vert.spv ../data/shaders/glsl/tutorial_raytracing/skybox.vert";
   system(command.c_str());

   command = glslc + " " + "-o ../data/shaders/glsl/tutorial_raytracing/skybox.frag.spv ../data/shaders/glsl/tutorial_raytracing/skybox.frag";
   system(command.c_str());

   if (destroyExistingStuff)
   {
      destroyRayTracingStuff(false);
      createRayTracingPipeline();
      createAndUpdateRayTracingDescriptorSets();
      _pushConstants.frameIndex = -1;

      destroyRasterizationStuff();
      createRasterizationPipeline();
      createAndUpdateRasterizationDescriptorSets();
      buildCommandBuffers();
   }
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
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, bindingIndex++)
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
      SBT Layout used in this sample:

      /-----------\
      | raygen    |
      |-----------|
      | miss      |
      |-----------|
      | hit       |
      \-----------/
   */
   _shaderBindingTable = new genesis::ShaderBindingTable(_device);
   _shaderBindingTable->addShader(getShadersPath() + "tutorial_raytracing/raygen.rgen.spv", genesis::ST_RT_RAYGEN);
   _shaderBindingTable->addShader(getShadersPath() + "tutorial_raytracing/miss.rmiss.spv", genesis::ST_RT_MISS);
   _shaderBindingTable->addShader(getShadersPath() + "tutorial_raytracing/closesthit.rchit.spv", genesis::ST_RT_CLOSEST_HIT);

   // create the ray tracing pipeline
   VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo{};
   rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
   rayTracingPipelineCreateInfo.stageCount = static_cast<uint32_t>(_shaderBindingTable->shaderStages().size());
   rayTracingPipelineCreateInfo.pStages = _shaderBindingTable->shaderStages().data();
   rayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(_shaderBindingTable->shaderGroups().size());
   rayTracingPipelineCreateInfo.pGroups = _shaderBindingTable->shaderGroups().data();
   rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
   rayTracingPipelineCreateInfo.layout = _rayTracingPipelineLayout;
   VK_CHECK_RESULT(_device->extensions().vkCreateRayTracingPipelinesKHR(_device->vulkanDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCreateInfo, nullptr, &_rayTracingPipeline));

   _shaderBindingTable->build(_rayTracingPipeline);
}

void TutorialRayTracing::createRasterizationPipeline()
{
   int bindingIndex = 0;
   std::vector<VkDescriptorSetLayoutBinding> set0Bindings =
   {
      genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   };
   VkDescriptorSetLayoutCreateInfo set0LayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(set0Bindings.data(), static_cast<uint32_t>(set0Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &set0LayoutInfo, nullptr, &_rasterizationDescriptorSetLayout));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _rasterizationDescriptorSetLayout, _cellManager->cell(0)->layout()->vulkanDescriptorSetLayout() };
   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::VulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   VkPushConstantRange pushConstant{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };
   pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
   pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

   VK_CHECK_RESULT(vkCreatePipelineLayout(_device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_rasterizationPipelineLayout));
   debugmarker::setName(_device->vulkanDevice(), _rasterizationPipelineLayout, "_pipelineLayout");

   // sky box
   vecDescriptorSetLayout = { _rasterizationDescriptorSetLayout, _skyBoxManager->cell(0)->layout()->vulkanDescriptorSetLayout() };
   pipelineLayoutCreateInfo.pSetLayouts = vecDescriptorSetLayout.data();
   pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)vecDescriptorSetLayout.size();

   VK_CHECK_RESULT(vkCreatePipelineLayout(_device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_rasterizationSkyBoxPipelineLayout));
   debugmarker::setName(_device->vulkanDevice(), _rasterizationSkyBoxPipelineLayout, "_rasterizationSkyBoxPipelineLayout");

   // bindings
   std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions = { genesis::VulkanInitializers::vertexInputBindingDescription(0, sizeof(genesis::Vertex), VK_VERTEX_INPUT_RATE_VERTEX) };

   // input descriptions
   int location = 0;
   std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
        genesis::VulkanInitializers::vertexInputAttributeDescription(0, location++, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, position))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, location++, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, normal))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, location++, VK_FORMAT_R32G32_SFLOAT, offsetof(genesis::Vertex, uv))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, location++, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, color))
   };

   // input state
   VkPipelineVertexInputStateCreateInfo vertexInputState = genesis::VulkanInitializers::pipelineVertexInputStateCreateInfo(vertexInputBindingDescriptions, vertexInputAttributeDescriptions);

   // input assembly
   VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = genesis::VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

   // viewport state
   VkPipelineViewportStateCreateInfo viewportState = genesis::VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);

   // rasterization state
   VkPipelineRasterizationStateCreateInfo rasterizationState = genesis::VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

   // multisample state
   VkPipelineMultisampleStateCreateInfo multisampleState = genesis::VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

   // depth stencil
   VkPipelineDepthStencilStateCreateInfo depthStencilState = genesis::VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

   // blend attachment
   VkPipelineColorBlendAttachmentState blendAttachmentState = genesis::VulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

   VkPipelineColorBlendStateCreateInfo colorBlendState = genesis::VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

   // dynamic states
   std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   VkPipelineDynamicStateCreateInfo dynamicState = genesis::VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

   VkRenderPass renderPass = (_dynamicRendering) ? nullptr : _renderPass->vulkanRenderPass();
   VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = genesis::VulkanInitializers::graphicsPipelineCreateInfo(_rasterizationPipelineLayout, renderPass);

   graphicsPipelineCreateInfo.pVertexInputState = &vertexInputState;
   graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
   graphicsPipelineCreateInfo.pViewportState = &viewportState;
   graphicsPipelineCreateInfo.pRasterizationState = &rasterizationState;
   graphicsPipelineCreateInfo.pMultisampleState = &multisampleState;
   graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilState;
   graphicsPipelineCreateInfo.pColorBlendState = &colorBlendState;
   graphicsPipelineCreateInfo.pDynamicState = &dynamicState;

   Shader* modelVertexShader = loadShader(getShadersPath() + "tutorial_raytracing/tutorial.vert.spv", genesis::ST_VERTEX_SHADER);
   Shader* modelPixelShader = loadShader(getShadersPath() + "tutorial_raytracing/tutorial.frag.spv", genesis::ST_FRAGMENT_SHADER);
   std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos = { modelVertexShader->pipelineShaderStageCreateInfo(), modelPixelShader->pipelineShaderStageCreateInfo() };
   graphicsPipelineCreateInfo.stageCount = (uint32_t)shaderStageInfos.size();
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();

   VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
   VkFormat colorFormat = _swapChain->colorFormat();
   if (_dynamicRendering)
   {
      pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
      pipelineRenderingCreateInfo.colorAttachmentCount = 1;
      pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
      pipelineRenderingCreateInfo.depthAttachmentFormat = _depthFormat;
      pipelineRenderingCreateInfo.stencilAttachmentFormat = _depthFormat;
      graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
   }
   
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_rasterizationPipeline));

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_rasterizationPipelineWireframe));
   rasterizationState.polygonMode = VK_POLYGON_MODE_FILL; // reset

   // next 2 are the skybox
   Shader* skyBoxVertexShader = loadShader(getShadersPath() + "tutorial_raytracing/skybox.vert.spv", genesis::ST_VERTEX_SHADER);
   Shader* skyBoxPixelShader = loadShader(getShadersPath() + "tutorial_raytracing/skybox.frag.spv", genesis::ST_FRAGMENT_SHADER);
   shaderStageInfos = { skyBoxVertexShader->pipelineShaderStageCreateInfo(), skyBoxPixelShader->pipelineShaderStageCreateInfo() };

   graphicsPipelineCreateInfo.layout = _rasterizationSkyBoxPipelineLayout;

   rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT; // cull the front facing polygons
   depthStencilState.depthWriteEnable = VK_FALSE;
   depthStencilState.depthTestEnable = VK_FALSE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxRasterizationPipeline));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxRasterizationPipeline, "_skyBoxPipeline");

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxRasterizationPipelineWireframe));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxRasterizationPipelineWireframe, "_skyBoxPipelineWireframe");
}

/*
Command buffer generation
*/
void TutorialRayTracing::rayTrace(int commandBufferIndex)
{
   VkCommandBufferBeginInfo cmdBufInfo = genesis::VulkanInitializers::commandBufferBeginInfo();

   VK_CHECK_RESULT(vkBeginCommandBuffer(_drawCommandBuffers[commandBufferIndex], &cmdBufInfo));

   vkCmdBindPipeline(_drawCommandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipeline);
   vkCmdBindDescriptorSets(_drawCommandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipelineLayout, 0, 1, &_rayTracingDescriptorSet, 0, 0);

   std::uint32_t firstSet = 1;
   vkCmdBindDescriptorSets(_drawCommandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipelineLayout, firstSet, std::uint32_t(_cellManager->cell(0)->layout()->descriptorSets().size()), _cellManager->cell(0)->layout()->descriptorSets().data(), 0, nullptr);

   ++_pushConstants.frameIndex;
   _pushConstants.clearColor = genesis::Vector4_32(1, 1, 1, 1);
   vkCmdPushConstants(
      _drawCommandBuffers[commandBufferIndex],
      _rayTracingPipelineLayout,
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
      0,
      sizeof(PushConstants),
      &_pushConstants);

   _device->extensions().vkCmdTraceRaysKHR(
      _drawCommandBuffers[commandBufferIndex]
      , &_shaderBindingTable->raygenEntry()
      , &_shaderBindingTable->missEntry()
      , &_shaderBindingTable->hitEntry()
      , &_shaderBindingTable->callableEntry()
      , _width
      , _height
      , 1);

   genesis::ImageTransitions transitions;
   // Prepare current swap chain image as transfer destination
   VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
   transitions.setImageLayout(_drawCommandBuffers[commandBufferIndex], _swapChain->image(commandBufferIndex), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

   // Prepare ray tracing output image as transfer source
   transitions.setImageLayout(_drawCommandBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

   VkImageCopy copyRegion{};
   copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
   copyRegion.srcOffset = { 0, 0, 0 };
   copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
   copyRegion.dstOffset = { 0, 0, 0 };
   copyRegion.extent = { _width, _height, 1 };
   vkCmdCopyImage(_drawCommandBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _swapChain->image(commandBufferIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

   // Transition swap chain image back for presentation
   transitions.setImageLayout(_drawCommandBuffers[commandBufferIndex], _swapChain->image(commandBufferIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresourceRange);

   // Transition ray tracing output image back to general layout
   transitions.setImageLayout(_drawCommandBuffers[commandBufferIndex], _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, subresourceRange);

   if (_dynamicRendering == false)
   {
      drawImgui(_drawCommandBuffers[commandBufferIndex], _frameBuffers[commandBufferIndex]);
   }

   VK_CHECK_RESULT(vkEndCommandBuffer(_drawCommandBuffers[commandBufferIndex]));
}

void TutorialRayTracing::buildRasterizationCommandBuffersDynamicRendering(void)
{
   VkCommandBufferBeginInfo commandBufferBeginInfo = VulkanInitializers::commandBufferBeginInfo();

   const VkViewport viewport = VulkanInitializers::viewport((float)_width, (float)_height, 0.0f, 1.0f);
   const VkRect2D scissor = VulkanInitializers::rect2D(_width, _height, 0, 0);

   ImageTransitions transitions;

   for (int32_t i = 0; i < _drawCommandBuffers.size(); ++i)
   {
      VK_CHECK_RESULT(vkBeginCommandBuffer(_drawCommandBuffers[i], &commandBufferBeginInfo));

      transitions.setImageLayout(_drawCommandBuffers[i], _swapChain->image(i)
         , VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
         , VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
         , VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT); // PPP: I Think this should be bottom of pipe

      // per the book: the outputs to the depth and stencil buffers occur as part of the late fragment test, so this along wit the early
      // fragment tests includes the depth and stencil outputs
      const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      transitions.setImageLayout(_drawCommandBuffers[i], _depthStencilImage->vulkanImage()
         , VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
         , VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 }
         , pipelineStageFlags, pipelineStageFlags);

      VkRenderingAttachmentInfo colorAttachment{};
      colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      colorAttachment.imageView = _swapChain->imageView(i);
      colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachment.clearValue = { 0.0f, 0.0f, 0.2f, 1.0f };

      VkRenderingAttachmentInfo depthStencilAttachment{};
      depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      depthStencilAttachment.imageView = _depthStencilImage->vulkanImageView();
      depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      depthStencilAttachment.clearValue = { 1.0f, 0.0f };

      VkRenderingInfo renderingInfo{};
      renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
      renderingInfo.renderArea = {0, 0, _width, _height};
      renderingInfo.layerCount = 1;
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &colorAttachment;
      renderingInfo.pDepthAttachment = &depthStencilAttachment;
      renderingInfo.pDepthAttachment = &depthStencilAttachment;
         
      _device->extensions().vkCmdBeginRenderingKHR(_drawCommandBuffers[i], &renderingInfo);

      // Update dynamic viewport state
      vkCmdSetViewport(_drawCommandBuffers[i], 0, 1, &viewport);

      // Update dynamic scissor state
      vkCmdSetScissor(_drawCommandBuffers[i], 0, 1, &scissor);

      // draw the sky box
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationSkyBoxPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);
      vkCmdPushConstants(_drawCommandBuffers[i], _rasterizationSkyBoxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      if (!_wireframe)
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipelineWireframe);
      }
      _skyBoxManager->cell(0)->draw(_drawCommandBuffers[i], _rasterizationSkyBoxPipelineLayout);

      // draw the model
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);
      vkCmdPushConstants(_drawCommandBuffers[i], _rasterizationPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      if (!_wireframe)
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineWireframe);
      }

      _cellManager->cell(0)->draw(_drawCommandBuffers[i], _rasterizationPipelineLayout);

      // draw the UI
      drawUI(_drawCommandBuffers[i]);

      _device->extensions().vkCmdEndRenderingKHR(_drawCommandBuffers[i]);

      transitions.setImageLayout(_drawCommandBuffers[i], _swapChain->image(i)
         , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
         , VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
         , VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT); // PPP: I think this should be top of pipe

      VK_CHECK_RESULT(vkEndCommandBuffer(_drawCommandBuffers[i]));
   }
}

void TutorialRayTracing::buildRasterizationCommandBuffers()
{
   VkCommandBufferBeginInfo cmdBufInfo = VulkanInitializers::commandBufferBeginInfo();

   // Set clear values for all framebuffer attachments with loadOp set to clear
   // We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
   VkClearValue clearValues[2];
   clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
   clearValues[1].depthStencil = { 1.0f, 0 };

   VkRenderPassBeginInfo renderPassBeginInfo = genesis::VulkanInitializers::renderPassBeginInfo();
   renderPassBeginInfo.renderPass = _renderPass->vulkanRenderPass();
   renderPassBeginInfo.renderArea.offset = { 0, 0 };
   renderPassBeginInfo.renderArea.extent = { _width, _height };

   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;

   const VkViewport viewport = genesis::VulkanInitializers::viewport((float)_width, (float)_height, 0.0f, 1.0f);
   const VkRect2D scissor = genesis::VulkanInitializers::rect2D(_width, _height, 0, 0);

   for (int32_t i = 0; i < _drawCommandBuffers.size(); ++i)
   {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = _frameBuffers[i];

      VK_CHECK_RESULT(vkBeginCommandBuffer(_drawCommandBuffers[i], &cmdBufInfo));

      // Start the first sub pass specified in our default render pass setup by the base class
      // This will clear the color and depth attachment
      vkCmdBeginRenderPass(_drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      // Update dynamic viewport state
      vkCmdSetViewport(_drawCommandBuffers[i], 0, 1, &viewport);

      // Update dynamic scissor state
      vkCmdSetScissor(_drawCommandBuffers[i], 0, 1, &scissor);

      // draw the sky box
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationSkyBoxPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);
      vkCmdPushConstants(_drawCommandBuffers[i], _rasterizationSkyBoxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      if (!_wireframe)
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipelineWireframe);
      }
      _skyBoxManager->cell(0)->draw(_drawCommandBuffers[i], _rasterizationSkyBoxPipelineLayout);

      // draw the model
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);
      vkCmdPushConstants(_drawCommandBuffers[i], _rasterizationPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      if (!_wireframe)
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineWireframe);
      }

      _cellManager->cell(0)->draw(_drawCommandBuffers[i], _rasterizationPipelineLayout);

      // draw the UI
      drawUI(_drawCommandBuffers[i]);

      vkCmdEndRenderPass(_drawCommandBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(_drawCommandBuffers[i]));
   }
}

std::string TutorialRayTracing::generateTimeStampedFileName(void)
{
   using namespace std;
   using namespace std::chrono;

   time_t tt = system_clock::to_time_t(system_clock::now());
   tm utc_tm = *gmtime(&tt);
   tm local_tm = *localtime(&tt);

   std::stringstream ss;
   ss << local_tm.tm_year + 1900 << '-';
   ss << local_tm.tm_mon + 1 << '-';
   ss << local_tm.tm_mday << '_';
   ss << local_tm.tm_hour;
   ss << local_tm.tm_min;
   if (local_tm.tm_sec < 10)
   {
      ss << "0";
   }
   ss << local_tm.tm_sec;

   std::string fileName = "c:\\temp\\" + ss.str() + ".png";
   //std::string fileName = "..\\screenshots\\" + ss.str() + ".png";
   return fileName;
}

void TutorialRayTracing::saveScreenShot(const std::string& fileName)
{
   genesis::ScreenShotUtility screenShotUtility(_device);
   screenShotUtility.takeScreenShot(fileName, _swapChain->image(_currentFrameBufferIndex), _swapChain->colorFormat()
      , _width, _height);
}

void TutorialRayTracing::nextRenderingMode(void)
{
   _mode = (RenderMode)((_mode + 1) % NUM_MODES);
   setupRenderPass();
   if (_mode == RASTERIZATION)
   {
      buildCommandBuffers();
   }
   _pushConstants.frameIndex = -1;
}

void TutorialRayTracing::keyPressed(uint32_t key)
{
   if (key == KEY_F5)
   {
      saveScreenShot(generateTimeStampedFileName());
   }
   else if (key == KEY_SPACE)
   {
      resetCamera();
      viewChanged();
   }
   else if (key == KEY_F4)
   {
      _settings.overlay = !_settings.overlay;
      buildCommandBuffers();
   }
   else if (key == KEY_R)
   {
      nextRenderingMode();
   }
   else if (key == KEY_P)
   {
      _pushConstants.pathTracer = (_pushConstants.pathTracer + 1) % 2;
      _pushConstants.frameIndex = -1;
   }
   else if (key == KEY_C)
   {
      _pushConstants.cosineSampling = (_pushConstants.cosineSampling + 1) % 2;
      _pushConstants.frameIndex = -1;
   }
}

void TutorialRayTracing::draw()
{
   VulkanApplication::prepareFrame();

   if (_mode == RAYTRACE)
   {
      rayTrace(_currentFrameBufferIndex);
   }
   else if (_mode == RASTERIZATION)
   {
      ++_pushConstants.frameIndex;
   }

   _submitInfo.commandBufferCount = 1;
   _submitInfo.pCommandBuffers = &_drawCommandBuffers[_currentFrameBufferIndex];
   VK_CHECK_RESULT(vkQueueSubmit(_device->graphicsQueue(), 1, &_submitInfo, VK_NULL_HANDLE));
   VulkanApplication::submitFrame();

#if 1
   if (_pushConstants.frameIndex == 15000)
   {
      saveScreenShot(generateTimeStampedFileName());
   }
#endif

   if (_autoTest)
   {
      static int currentScreenshot = 0;
      if (_pushConstants.frameIndex == 5000)
      {
         std::string fileName;
         if (currentScreenshot == 0)
         {
            fileName = "..\\autotest\\" + _mainModel + "_raytrace" + ".png";
         }
         else if (currentScreenshot == 1)
         {
            fileName = "..\\autotest\\" + _mainModel + "_rasterization" + ".png";
         }
         else if (currentScreenshot == 2)
         {
            fileName = "..\\autotest\\" + _mainModel + "_rasterization_emulated_by_raytrace" + ".png";
         }
         saveScreenShot(fileName);
         ++currentScreenshot;
         // If the last for this model, switch to n.v single bounce path tracer
         if (currentScreenshot == 2)
         {
            _pushConstants.pathTracer = 0;
         }
         // last one. send quit message
         else if (currentScreenshot == 3)
         {
            onKeyboard(256, -1, 1, -1);
         }
         nextRenderingMode();
      }
   }
}

void TutorialRayTracing::render()
{
   if (!_prepared)
   {
      return;
   }
   draw();
}

void TutorialRayTracing::viewChanged()
{
   _pushConstants.frameIndex = -1;
   updateSceneUbo();
}

void TutorialRayTracing::updateSceneUbo()
{
   SceneUbo ubo;
   ubo.viewMatrix = _camera.matrices.view;
   ubo.viewMatrixInverse = glm::inverse(_camera.matrices.view);

   ubo.projectionMatrix = _camera.matrices.perspective;
   ubo.projectionMatrixInverse = glm::inverse(_camera.matrices.perspective);

   ubo.vertexSizeInBytes = sizeof(genesis::Vertex);

   uint8_t* data = (uint8_t*)_sceneUbo->stagingBuffer();
   memcpy(data, &ubo, sizeof(SceneUbo));
   _sceneUbo->syncToGpu(false);
}

void TutorialRayTracing::createSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

void TutorialRayTracing::createCells(void)
{
   std::string gltfModel;
   std::string gltfModel2;

   if (_mainModel == "sponza")
   {
      gltfModel = getAssetsPath() + "models/sponza/sponza.gltf";
   }
   else if (_mainModel == "venus")
   {
      gltfModel = getAssetsPath() + "models/venus.gltf";
   }
   else if (_mainModel == "cornell")
   {
      gltfModel = getAssetsPath() + "models/cornellBox_used_for_comparison_gen_vs_ref.gltf";
   }
   else if (_mainModel == "sphere")
   {
      gltfModel = getAssetsPath() + "models/sphere.gltf";
   }
   else if (_mainModel == "bathroom")
   {
      gltfModel = getAssetsPath() + "models/bathroom/LAZIENKA.gltf";
   }
   else
   {
      gltfModel = _mainModel;
   }

   _cellManager = new genesis::CellManager(_device, _glTFLoadingFlags);

   _cellManager->addInstance(gltfModel, mat4());

#if 0
   gltfModel2 = getAssetsPath() + "../../glTF-Sample-Models/2.0//WaterBottle//glTF/WaterBottle.gltf";

   _cellManager->addInstance(gltfModel2, glm::translate(glm::mat4(), glm::vec3(-1, -1, 0.0f)));
   _cellManager->addInstance(gltfModel2, glm::translate(glm::mat4(), glm::vec3(-3, -2.0f, 0.0f)));
#endif

   _cellManager->buildTlases();
   _cellManager->buildDrawBuffers();
   _cellManager->buildLayouts();
}

void TutorialRayTracing::createSkyBox(void)
{
   const uint32_t glTFLoadingFlags = genesis::VulkanGltfModel::PreTransformVertices ;
   _skyBoxManager = new genesis::CellManager(_device, glTFLoadingFlags);
   _skyBoxManager->addInstance(getAssetsPath() + "models/cube.gltf", glm::mat4());

   _skyBoxManager->buildDrawBuffers();
   _skyBoxManager->buildLayouts();

   _skyCubeMapImage = new genesis::Image(_device);
#if (defined SKYBOX_YOKOHOMA)
   _pushConstants.environmentMapCoordTransform.x = -1;
   _pushConstants.environmentMapCoordTransform.y = +1;
   _skyCubeMapImage->loadFromFileCubeMap(getAssetsPath() + "textures/cubemap_yokohama_rgba.ktx");
#endif

#if (defined SKYBOX_PISA)
   _skyCubeMapImage->loadFromFileCubeMap(getAssetsPath() + "textures/hdr/pisa_cube.ktx");
#endif
   _skyCubeMapTexture = new genesis::Texture(_skyCubeMapImage);
}

void TutorialRayTracing::createScene()
{
   _glTFLoadingFlags = genesis::VulkanGltfModel::PreTransformVertices ;
   createCells();
   createSkyBox();
}

void TutorialRayTracing::createPipelines()
{
   createRayTracingPipeline();
   createRasterizationPipeline();
}

void TutorialRayTracing::buildCommandBuffers()
{
   if (_dynamicRendering)
   {
      buildRasterizationCommandBuffersDynamicRendering();
   }
   else
   {
      buildRasterizationCommandBuffers();
   }
}

void TutorialRayTracing::prepare()
{
   VulkanApplication::prepare();
   reloadShaders(false);
   createScene();
   createStorageImages();
   createSceneUbo();
   createPipelines();
   createAndUpdateDescriptorSets();
   buildCommandBuffers();
   _prepared = true;
}

void TutorialRayTracing::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
{
   if (overlay->header("Settings"))
   {
      if (overlay->checkBox("wireframe", &_wireframe))
      {
         // no op
      }
      if (overlay->sliderFloat("LOD bias", &_pushConstants.textureLodBias, 0.0f, 1.0f))
      {
         _pushConstants.frameIndex = -1; // need to start tracing again if ray tracing
      }
      if (overlay->sliderFloat("reflectivity", &_pushConstants.reflectivity, 0, 1))
      {
         // no op
      }
      if (overlay->sliderFloat("sky value", &_pushConstants.contributionFromEnvironment, 0, 100))
      {
         _pushConstants.frameIndex = -1;
      }
      if (overlay->button("Reload Shaders"))
      {
         reloadShaders(true);
      }
      static std::vector<std::string> items =
      { "none", "albedo", "emissive", "roughness", "metalness", "ao", "normal map", "geometry normals", "normal map normals" };
      if (overlay->comboBox("component", &_pushConstants.materialComponentViz, items))
      {
         _pushConstants.frameIndex = -1;
      }
   }
}

void TutorialRayTracing::drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
{
   if (_mode == RASTERIZATION)
   {
      return;
   }
   VkClearValue clearValues[2];
   clearValues[0].color = _defaultClearColor;
   clearValues[1].depthStencil = { 1.0f, 0 };

   VkRenderPassBeginInfo renderPassBeginInfo = genesis::VulkanInitializers::renderPassBeginInfo();
   renderPassBeginInfo.renderPass = _renderPass->vulkanRenderPass();
   renderPassBeginInfo.renderArea.offset = { 0, 0 };
   renderPassBeginInfo.renderArea.extent = { _width, _height };
   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;
   renderPassBeginInfo.framebuffer = framebuffer;

   vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
   VulkanApplication::drawUI(commandBuffer);
   vkCmdEndRenderPass(commandBuffer);
}

void TutorialRayTracing::setupRenderPass()
{
   delete _renderPass;
   if (_mode == RAYTRACE)
   {
      if (_dynamicRendering == false)
      {
         _renderPass = new genesis::RenderPass(_device, _swapChain->colorFormat(), _depthFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
      }
   }
   else
   {
      VulkanApplication::setupRenderPass();
   }
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
   _intermediateImage = new genesis::StorageImage(_device, VK_FORMAT_R32G32B32A32_SFLOAT, _width, _height
      , VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL);

   // final image is used for presentation. So, its the same format as the swap chain
   _finalImageToPresent = new genesis::StorageImage(_device, _swapChain->colorFormat(), _width, _height
      , VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL);

   genesis::ImageTransitions transitions;
   VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

   transitions.setImageLayout(commandBuffer, _intermediateImage->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
   transitions.setImageLayout(commandBuffer, _finalImageToPresent->vulkanImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

   _device->flushCommandBuffer(commandBuffer);
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

void TutorialRayTracing::onDrop(const std::vector<std::string>& filesDropped)
{
   if (filesDropped.size() == 0)
   {
      return;
   }
   const std::string& fileName = filesDropped[0];
   if (fileName.find(".gltf") == std::string::npos && fileName.find(".glb") == std::string::npos)
   {
      return;
   }
   destroyRayTracingStuff(false);
   destroyRasterizationStuff();

   delete _cellManager;
   _cellManager = nullptr;

   _mainModel = fileName;
   createCells();
   createRayTracingPipeline();
   createAndUpdateRayTracingDescriptorSets();
   _pushConstants.frameIndex = -1;

   createRasterizationPipeline();
   createAndUpdateRasterizationDescriptorSets();
   buildCommandBuffers();
   resetCamera();
}
