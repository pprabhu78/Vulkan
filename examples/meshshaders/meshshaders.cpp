/*
* Mesh Shaders example
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "meshshaders.h"

#include "Device.h"
#include "PhysicalDevice.h"
#include "Texture.h"
#include "Buffer.h"
#include "Shader.h"
#include "VulkanInitializers.h"
#include "VulkanMeshlet.h"
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

void MeshShaders::resetCamera()
{
   _camera.type = Camera::CameraType::lookat;
   _camera.setPosition(glm::vec3(0.0f, 0.0f, -350.5f));
   _camera.setRotation(glm::vec3(0.0f));
   _camera.setPerspective(60.0f, (float)_width / (float)_height, 1.0f, 1000);
}

MeshShaders::MeshShaders()
   : _pushConstants{}
{
   _settings.overlay = false;

   _title = "genesis: mesh shaders";

   resetCamera();

   // Require Vulkan 1.3
   apiVersion = VK_API_VERSION_1_3;

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

   // If this is not enabled, there is validation error
   // If this is not enabled, there vkCmdDrawMeshTasksEXT is null
   _enabledPhysicalDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
}

#define  ADD_NEXT(previous, current) current.pNext = &previous

void MeshShaders::enableFeatures()
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

   _enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
   _enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

   // If this is not enabled, there is validation error, but mesh shaders work
   ADD_NEXT(_enabledBufferDeviceAddressFeatures, _physicalDeviceMeshShaderFeaturesEXT);
   _physicalDeviceMeshShaderFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
   _physicalDeviceMeshShaderFeaturesEXT.meshShader = VK_TRUE;
   _physicalDeviceMeshShaderFeaturesEXT.taskShader = VK_TRUE;

   ADD_NEXT(_physicalDeviceMeshShaderFeaturesEXT, _physicalDeviceMaintenance4Features);
   _physicalDeviceMaintenance4Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES;
   _physicalDeviceMaintenance4Features.maintenance4 = VK_TRUE;

   ADD_NEXT(_physicalDeviceMaintenance4Features, _physicalDeviceDescriptorIndexingFeatures);
   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

   deviceCreatepNextChain = &_physicalDeviceDescriptorIndexingFeatures;
}

void MeshShaders::destroyRasterizationStuff()
{
   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxRasterizationPipeline, nullptr);
   _skyBoxRasterizationPipeline = 0;

   vkDestroyPipeline(_device->vulkanDevice(), _rasterizationPipeline, nullptr);
   _rasterizationPipeline = 0;

   vkDestroyPipelineLayout(_device->vulkanDevice(), _rasterizationPipelineLayout, nullptr);
   _rasterizationPipelineLayout = 0;

   vkDestroyPipelineLayout(_device->vulkanDevice(), _rasterizationSkyBoxPipelineLayout, nullptr);
   _rasterizationSkyBoxPipelineLayout = 0;

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _rasterizationDescriptorSetLayout, nullptr);
   _rasterizationDescriptorSetLayout = 0;
   vkDestroyDescriptorPool(_device->vulkanDevice(), _rasterizationDescriptorPool, nullptr);
   _rasterizationDescriptorPool = 0;

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _meshShadersDescriptorSetLayout, nullptr);
   _meshShadersDescriptorSetLayout = 0;

   vkDestroyDescriptorPool(_device->vulkanDevice(), _meshShadersDescriptorPool, nullptr);
   _meshShadersDescriptorPool = 0;
}

void MeshShaders::destroyCommonStuff()
{
   delete _skyBoxManager;

   delete _sceneUbo;

   delete _skyCubeMapTexture;
   delete _skyCubeMapImage;
}

MeshShaders::~MeshShaders()
{
   delete _model;
   destroyRasterizationStuff();
   destroyCommonStuff();
}

void MeshShaders::createAndUpdateMeshShaderDescriptorSets()
{
   std::vector<VkDescriptorPoolSize> poolSizes = 
   {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}
   };

   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_meshShadersDescriptorPool));

   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(_meshShadersDescriptorPool, &_meshShadersDescriptorSetLayout, 1);
   vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &_meshShadersDescriptorSet);

   int bindingIndex = 0;
   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
     genesis::VulkanInitializers::writeDescriptorSet(_meshShadersDescriptorSet,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++,&_model->vertexBuffers()[0]->descriptor())
   , genesis::VulkanInitializers::writeDescriptorSet(_meshShadersDescriptorSet,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++,&_model->meshletBuffers()[0]->descriptor())
   , genesis::VulkanInitializers::writeDescriptorSet(_meshShadersDescriptorSet,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++,&_model->uniqueVertexIndices()[0]->descriptor())
   , genesis::VulkanInitializers::writeDescriptorSet(_meshShadersDescriptorSet,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingIndex++,&_model->primitiveIndices()[0]->descriptor())
   , genesis::VulkanInitializers::writeDescriptorSet(_meshShadersDescriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++,&_sceneUbo->descriptor())
   };

   vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void MeshShaders::createAndUpdateRasterizationDescriptorSets()
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

void MeshShaders::createAndUpdateDescriptorSets()
{
   createAndUpdateRasterizationDescriptorSets();
   createAndUpdateMeshShaderDescriptorSets();
}

void MeshShaders::reloadShaders(bool destroyExistingStuff)
{
   std::string strVulkanDir = getenv("VULKAN_SDK");
   std::string glslValidator = strVulkanDir + "\\bin\\glslangValidator.exe";

   std::string command = glslValidator + " " + "--target-env vulkan1.3 -V -o ../data/shaders/glsl/meshshaders/meshshader.task.spv ../data/shaders/glsl/meshshaders/meshshader.task";
   system(command.c_str());

   command = glslValidator + " " + "--target-env vulkan1.3 -V -o ../data/shaders/glsl/meshshaders/meshshader.mesh.spv ../data/shaders/glsl/meshshaders/meshshader.mesh";
   system(command.c_str());

   command = glslValidator + " " + "--target-env vulkan1.3 -V -o ../data/shaders/glsl/meshshaders/meshshader.frag.spv ../data/shaders/glsl/meshshaders/meshshader.frag";
   system(command.c_str());

   std::string glslc = strVulkanDir + "\\bin\\glslc.exe";
   command = glslc + " " + "-o ../data/shaders/glsl/meshshaders/skybox.vert.spv ../data/shaders/glsl/meshshaders/skybox.vert";
   system(command.c_str());

   command = glslc + " " + "-o ../data/shaders/glsl/meshshaders/skybox.frag.spv ../data/shaders/glsl/meshshaders/skybox.frag";
   system(command.c_str());

   if (destroyExistingStuff)
   {
      _pushConstants.frameIndex = -1;

      destroyRasterizationStuff();
      createRasterizationPipeline();
      createAndUpdateMeshShaderDescriptorSets();
      buildCommandBuffers();
   }
}

void MeshShaders::createRasterizationPipeline()
{
   const VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

   int bindingIndex = 0;
   std::vector<VkDescriptorSetLayoutBinding> set0Bindings =
   {
      genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   };
   VkDescriptorSetLayoutCreateInfo set0LayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(set0Bindings.data(), static_cast<uint32_t>(set0Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &set0LayoutInfo, nullptr, &_rasterizationDescriptorSetLayout));

   bindingIndex = 0;
   std::vector<VkDescriptorSetLayoutBinding> set1Bindings =
   {
     genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   , genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   , genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   , genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   , genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   };
   VkDescriptorSetLayoutCreateInfo set1LayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(set1Bindings.data(), static_cast<uint32_t>(set1Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &set1LayoutInfo, nullptr, &_meshShadersDescriptorSetLayout));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _rasterizationDescriptorSetLayout, _meshShadersDescriptorSetLayout };
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

   VkPipelineRasterizationStateCreateInfo rasterizationState = genesis::VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 0);

   VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = genesis::VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

   VkPipelineViewportStateCreateInfo viewportState = genesis::VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);
   VkPipelineMultisampleStateCreateInfo multisampleState = genesis::VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
   VkPipelineDepthStencilStateCreateInfo depthStencilState = genesis::VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
   VkPipelineColorBlendAttachmentState blendAttachmentState = genesis::VulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
   VkPipelineColorBlendStateCreateInfo colorBlendState = genesis::VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

   // dynamic states
   std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   VkPipelineDynamicStateCreateInfo dynamicState = genesis::VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

   VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = genesis::VulkanInitializers::graphicsPipelineCreateInfo(_rasterizationPipelineLayout, _renderPass->vulkanRenderPass());

   graphicsPipelineCreateInfo.pVertexInputState = &vertexInputState;
   graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
   graphicsPipelineCreateInfo.pViewportState = &viewportState;
   graphicsPipelineCreateInfo.pRasterizationState = &rasterizationState;
   graphicsPipelineCreateInfo.pMultisampleState = &multisampleState;
   graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilState;
   graphicsPipelineCreateInfo.pColorBlendState = &colorBlendState;
   graphicsPipelineCreateInfo.pDynamicState = &dynamicState;

   //Shader* modelTaskShader = loadShader(getShadersPath() + "meshshaders/meshshader.task.spv", genesis::ST_TASK_SHADER);
   Shader* modelMeshShader = loadShader(getShadersPath() + "meshshaders/meshshader.mesh.spv", genesis::ST_MESH_SHADER);
   Shader* modelFragmentShader = loadShader(getShadersPath() + "meshshaders/meshshader.frag.spv", genesis::ST_FRAGMENT_SHADER);
   std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos = { /*modelTaskShader->pipelineShaderStageCreateInfo(), */modelMeshShader->pipelineShaderStageCreateInfo(), modelFragmentShader->pipelineShaderStageCreateInfo() };
   graphicsPipelineCreateInfo.stageCount = (uint32_t)shaderStageInfos.size();
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_rasterizationPipeline));

   // sky box
   Shader* skyBoxVertexShader = loadShader(getShadersPath() + "meshshaders/skybox.vert.spv", genesis::ST_VERTEX_SHADER);
   Shader* skyBoxPixelShader = loadShader(getShadersPath() + "meshshaders/skybox.frag.spv", genesis::ST_FRAGMENT_SHADER);
   shaderStageInfos = { skyBoxVertexShader->pipelineShaderStageCreateInfo(), skyBoxPixelShader->pipelineShaderStageCreateInfo() };
   graphicsPipelineCreateInfo.stageCount = (uint32_t)shaderStageInfos.size();
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();
   graphicsPipelineCreateInfo.layout = _rasterizationSkyBoxPipelineLayout;

   rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT; // cull the front facing polygons
   rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // cull the front facing polygons
   depthStencilState.depthWriteEnable = VK_FALSE;
   depthStencilState.depthTestEnable = VK_FALSE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxRasterizationPipeline));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxRasterizationPipeline, "_skyBoxPipeline");
}

void MeshShaders::buildRasterizationCommandBuffers()
{
   VkCommandBufferBeginInfo cmdBufInfo = {};
   cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cmdBufInfo.pNext = nullptr;

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

      vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipeline);
      _skyBoxManager->cell(0)->draw(_drawCommandBuffers[i], _rasterizationSkyBoxPipelineLayout);

      // draw the model
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);
      vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineLayout, 1, 1, &_meshShadersDescriptorSet, 0, nullptr);

      vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipeline);

      int numMeshlets = _model->meshes()[0].MeshletSubsets[0].Count;
      _device->extensions().vkCmdDrawMeshTasksEXT(_drawCommandBuffers[i], numMeshlets, 1, 1);

      // draw the UI
      //drawUI(_drawCommandBuffers[i]);

      vkCmdEndRenderPass(_drawCommandBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(_drawCommandBuffers[i]));
   }
}

std::string MeshShaders::generateTimeStampedFileName(void)
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

void MeshShaders::saveScreenShot(const std::string& fileName)
{
   genesis::ScreenShotUtility screenShotUtility(_device);
   screenShotUtility.takeScreenShot(fileName, _swapChain->image(_currentFrameBufferIndex), _swapChain->colorFormat()
      , _width, _height);
}

void MeshShaders::keyPressed(uint32_t key)
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

void MeshShaders::draw()
{
   VulkanApplication::prepareFrame();

   ++_pushConstants.frameIndex;

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
}

void MeshShaders::render()
{
   if (!_prepared)
   {
      return;
   }
   draw();
}

void MeshShaders::viewChanged()
{
   _pushConstants.frameIndex = -1;
   updateSceneUbo();
}

void MeshShaders::updateSceneUbo()
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

void MeshShaders::createSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

void MeshShaders::createScene()
{
   _model = new VulkanMeshletModel(_device);
   _model->loadfromFile(getAssetsPath() + "models/lucy.bin");

   const uint32_t glTFLoadingFlags = genesis::VulkanGltfModel::PreTransformVertices;
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

void MeshShaders::createPipelines()
{
   createRasterizationPipeline();
}

void MeshShaders::buildCommandBuffers()
{
   buildRasterizationCommandBuffers();
}

void MeshShaders::prepare()
{
   VulkanApplication::prepare();
   reloadShaders(false);
   createScene();
   createSceneUbo();
   createPipelines();
   createAndUpdateDescriptorSets();
   buildCommandBuffers();
   _prepared = true;
}

void MeshShaders::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
{
   if (overlay->header("Settings"))
   {

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

void MeshShaders::drawImgui(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
{
   // does not work with rasterization?
   return;
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

void MeshShaders::setupRenderPass()
{
   VulkanApplication::setupRenderPass();
}

void MeshShaders::windowResized()
{
   _pushConstants.frameIndex = -1;
}
