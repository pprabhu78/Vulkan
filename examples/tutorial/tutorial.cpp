/*
* Vulkan Example - Basic hardware accelerated ray tracing example
*
* Copyright (C) 2019-2020 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "tutorial.h"


#include "Device.h"
#include "PhysicalDevice.h"
#include "Texture.h"
#include "Buffer.h"
#include "Shader.h"
#include "Image.h"
#include "RenderPass.h"

#include "IndirectLayout.h"
#include "Cell.h"
#include "CellManager.h"

#include "VulkanInitializers.h"
#include "VulkanGltf.h"
#include "VulkanDebug.h"
#include "ScreenShotUtility.h"

#include "GenAssert.h"

#include <chrono>
#include <sstream>

//#define VENUS 1
#define SPONZA 1
//#define CORNELL 1
//#define SPHERE 1

//#define SKYBOX_PISA 1
#define SKYBOX_YOKOHOMA 1

using namespace genesis;
using namespace genesis::tools;

void Tutorial::resetCamera(void)
{
#if (defined VENUS)
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
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

Tutorial::Tutorial()
: _pushConstants{}
{
   title = "genesis: tutorial";

   _pushConstants.clearColor = glm::vec4(0, 0, 0, 0);
   _pushConstants.environmentMapCoordTransform = glm::vec4(1, 1, 1, 1);

   _pushConstants.frameIndex = -1;
   _pushConstants.textureLodBias = 0;
   _pushConstants.reflectivity = 0;

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
}

#define  ADD_NEXT(previous, current) current.pNext = &previous

void Tutorial::enableFeatures()
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

   _enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
   _enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
   ADD_NEXT(_enabledBufferDeviceAddressFeatures, _enabledRayTracingPipelineFeatures);

   _enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
   _enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
   ADD_NEXT(_enabledRayTracingPipelineFeatures, _enabledAccelerationStructureFeatures);

   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
   ADD_NEXT(_enabledAccelerationStructureFeatures, _physicalDeviceDescriptorIndexingFeatures);

   _physicalDeviceShaderClockFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
   _physicalDeviceShaderClockFeaturesKHR.shaderDeviceClock = true;
   _physicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock = true;
   ADD_NEXT(_physicalDeviceDescriptorIndexingFeatures, _physicalDeviceShaderClockFeaturesKHR);

   deviceCreatepNextChain = &_physicalDeviceShaderClockFeaturesKHR;
}

Tutorial::~Tutorial()
{
   for (genesis::Shader* shader : _shaders)
   {
      delete shader;
   }
   vkDestroyPipeline(_device->vulkanDevice(), _rasterizationPipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _rasterizationPipelineWireframe, nullptr);

   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxRasterizationPipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxRasterizationPipelineWireframe, nullptr);

   vkDestroyPipelineLayout(_device->vulkanDevice(), _rasterizationPipelineLayout, nullptr);
   

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _rasterizationDescriptorSetLayout, nullptr);
   vkDestroyDescriptorPool(_device->vulkanDevice(), _rasterizationDescriptorPool, nullptr);

   delete _cellManager;

   delete _sceneUbo;

   delete _gltfSkyboxModel;
   delete _skyCubeMapTexture;
   delete _skyCubeMapImage;
}

void Tutorial::createAndUpdateRasterizationDescriptorSets(void)
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

void Tutorial::createRasterizationPipeline()
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
   std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
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

   Shader* modelVertexShader = loadShader(getShadersPath() + "tutorial/tutorial.vert.spv", genesis::ST_VERTEX_SHADER);
   Shader* modelPixelShader = loadShader(getShadersPath() + "tutorial/tutorial.frag.spv", genesis::ST_FRAGMENT_SHADER);
   std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos = { modelVertexShader->pipelineShaderStageCreateInfo(), modelPixelShader->pipelineShaderStageCreateInfo() };
   graphicsPipelineCreateInfo.stageCount = (uint32_t)shaderStageInfos.size();
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_rasterizationPipeline));

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_rasterizationPipelineWireframe));
   rasterizationState.polygonMode = VK_POLYGON_MODE_FILL; // reset

   // next 2 are the skybox
   Shader* skyBoxVertexShader = loadShader(getShadersPath() + "tutorial/skybox.vert.spv", genesis::ST_VERTEX_SHADER);
   Shader* skyBoxPixelShader = loadShader(getShadersPath() + "tutorial/skybox.frag.spv", genesis::ST_FRAGMENT_SHADER);
   shaderStageInfos = { skyBoxVertexShader->pipelineShaderStageCreateInfo(), skyBoxPixelShader->pipelineShaderStageCreateInfo() };

   rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT; // cull the front facing polygons
   depthStencilState.depthWriteEnable = VK_FALSE;
   depthStencilState.depthTestEnable = VK_FALSE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxRasterizationPipeline));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxRasterizationPipeline, "_skyBoxPipeline");

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxRasterizationPipelineWireframe));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxRasterizationPipelineWireframe, "_skyBoxPipelineWireframe");
}


void Tutorial::buildCommandBuffers()
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
   renderPassBeginInfo.renderArea.extent = { width, height };

   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;

   const VkViewport viewport = genesis::VulkanInitializers::viewport((float)width, (float)height, 0.0f, 1.0f);
   const VkRect2D scissor = genesis::VulkanInitializers::rect2D(width, height, 0, 0);

   for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
   {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = frameBuffers[i];

      VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

      // Start the first sub pass specified in our default render pass setup by the base class
      // This will clear the color and depth attachment
      vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      // Update dynamic viewport state
      vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

      // Update dynamic scissor state
      vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineLayout, 0, 1, &_rasterizationDescriptorSet, 0, nullptr);

      vkCmdPushConstants(drawCmdBuffers[i], _rasterizationPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      // draw the sky box
      if (!_wireframe)
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxRasterizationPipelineWireframe);
      }
#pragma message("PPP: TO DO: skybox")
      //_indirectLayout->draw(drawCmdBuffers[i], _pipelineLayout, _gltfModel);

      // draw the model
      if (!_wireframe)
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipeline);
      }
      else
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterizationPipelineWireframe);
      }

      _cellManager->cell(0)->draw(drawCmdBuffers[i], _rasterizationPipelineLayout);

      // draw the UI
      drawUI(drawCmdBuffers[i]);

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
   }
}


void Tutorial::saveScreenShot(void)
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

   std::string fileName = ss.str();

   genesis::ScreenShotUtility screenShotUtility(_device);
   screenShotUtility.takeScreenShot("..\\screenshots\\" + fileName + ".png"
      , swapChain.images[currentBuffer], swapChain.colorFormat
      , width, height);
}

void Tutorial::keyPressed(uint32_t key)
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

void Tutorial::draw()
{
   VulkanExampleBase::prepareFrame();

   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
   VK_CHECK_RESULT(vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
   VulkanExampleBase::submitFrame();
}

void Tutorial::render()
{
   if (!prepared)
   {
      return;
   }
   draw();
}

void Tutorial::viewChanged()
{
   _pushConstants.frameIndex = -1;
   updateSceneUbo();
}

void Tutorial::updateSceneUbo(void)
{
   SceneUbo ubo;
   ubo.viewMatrix = camera.matrices.view;
   ubo.viewMatrixInverse = glm::inverse(camera.matrices.view);

   ubo.projectionMatrix = camera.matrices.perspective;
   ubo.projectionMatrixInverse = glm::inverse(camera.matrices.perspective);

   ubo.vertexSizeInBytes = sizeof(genesis::Vertex);

   uint8_t* data = (uint8_t*)_sceneUbo->stagingBuffer();
   memcpy(data, &ubo, sizeof(SceneUbo));
   _sceneUbo->syncToGpu(false);
}

void Tutorial::createSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

void Tutorial::createScene(void)
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

   _cellManager->buildDrawBuffers();
   _cellManager->buildLayouts();

   _gltfSkyboxModel = new genesis::VulkanGltfModel(_device, false);
   _gltfSkyboxModel->loadFromFile(getAssetsPath() + "models/cube.gltf", glTFLoadingFlags);

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

#pragma message("PPP: TO DO: skybox")
#if 0
   _indirectLayout = new genesis::IndirectLayout(_device);
   _indirectLayout->build({ _gltfModel });
   _indirectLayout->buildDrawBuffers({ _gltfModel });
#endif
}

void Tutorial::prepare()
{
   VulkanExampleBase::prepare();
   createScene();
   createSceneUbo();
   createRasterizationPipeline();
   createAndUpdateRasterizationDescriptorSets();
   buildCommandBuffers();
   
   prepared = true;
}
void Tutorial::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
{
   if (overlay->header("Settings")) {
      if (overlay->checkBox("wireframe", &_wireframe)) {
      }
      if (overlay->sliderFloat("reflectivity", &_pushConstants.reflectivity, 0, 1)) {
      }
      if (overlay->sliderFloat("roughness", &_pushConstants.textureLodBias, 0, 1)) {
      }     
   }
}

genesis::Shader* Tutorial::loadShader(const std::string& shaderFile, genesis::ShaderType shaderType)
{
   genesis::Shader* shader = new genesis::Shader(_device);
   shader->loadFromFile(shaderFile, shaderType);
   if (shader->valid() == false)
   {
      std::cout << "error loading shader" << std::endl;
      delete shader;
      return nullptr;
   }
   _shaders.push_back(shader);
   return shader;
}

