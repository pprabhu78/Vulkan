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

#include "GenAssert.h"

//#define VENUS 1
#define SPONZA 1
//#define SPHERE 1

//#define SKYBOX_PISA 1
#define SKYBOX_YOKOHOMA 1

using namespace genesis;
using namespace genesis::tools;

Tutorial::Tutorial()
{
   _pushConstants.clearColor = glm::vec4(0, 0, 0, 0);
   _pushConstants.environmentMapCoordTransform = glm::vec4(1, 1, 1, 1);

   _pushConstants.frameIndex = 0;
   _pushConstants.textureLodBias = 0;
   _pushConstants.reflectivity = 0;

   title = "genesis: tutorial";
#if (defined VENUS)
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
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
#endif
   // Require Vulkan 1.2
   apiVersion = VK_API_VERSION_1_2;

   _enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
   _enabledPhysicalDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);

   _enabledPhysicalDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
}

void Tutorial::enableFeatures()
{
   // This is required for 64 bit math
   _physicalDevice->enabledPhysicalDeviceFeatures().shaderInt64 = true;

   // This is required for multi draw indirect
   if (_physicalDevice->physicalDeviceFeatures().multiDrawIndirect) {
      _physicalDevice->enabledPhysicalDeviceFeatures().multiDrawIndirect = VK_TRUE;
   }

   // Enable anisotropic filtering if supported
   if (_physicalDevice->physicalDeviceFeatures().samplerAnisotropy) {
      _physicalDevice->enabledPhysicalDeviceFeatures().samplerAnisotropy = VK_TRUE;
   }

   // This is required for wireframe display
   if (_physicalDevice->physicalDeviceFeatures().fillModeNonSolid)
   {
      _physicalDevice->enabledPhysicalDeviceFeatures().fillModeNonSolid = VK_TRUE;
   }

   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

   _enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
   _enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
   _enabledBufferDeviceAddressFeatures.pNext = &_physicalDeviceDescriptorIndexingFeatures;

   deviceCreatepNextChain = &_enabledBufferDeviceAddressFeatures;
}

Tutorial::~Tutorial()
{
   for (genesis::Shader* shader : _shaders)
   {
      delete shader;
   }
   vkDestroyPipeline(_device->vulkanDevice(), _pipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _pipelineWireframe, nullptr);

   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxPipeline, nullptr);
   vkDestroyPipeline(_device->vulkanDevice(), _skyBoxPipelineWireframe, nullptr);

   vkDestroyPipelineLayout(_device->vulkanDevice(), _pipelineLayout, nullptr);

   delete _cellManager;

   delete _sceneUbo;

   delete _gltfSkyboxModel;
   delete _skyCubeMapTexture;
   delete _skyCubeMapImage;

   vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _setLayout0, nullptr);
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

      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet0, 0, nullptr);

      vkCmdPushConstants(drawCmdBuffers[i], _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &_pushConstants);

      // draw the sky box
      if (!_wireframe)
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxPipeline);
      }
      else
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxPipelineWireframe);
      }
#pragma message("PPP: TO DO: skybox")
      //_indirectLayout->draw(drawCmdBuffers[i], _pipelineLayout, _gltfModel);

      // draw the model
      if (!_wireframe)
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
      }
      else
      {
         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineWireframe);
      }

      _cellManager->cell(0)->draw(drawCmdBuffers[i], _pipelineLayout);
      
      // draw the UI
      drawUI(drawCmdBuffers[i]);

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
   }

}

void Tutorial::updateSceneUbo(void)
{
   SceneUbo ubo;
   ubo.viewMatrix = camera.matrices.view;
   ubo.projectionMatrix = camera.matrices.perspective;

   ubo.viewMatrixInverse = glm::inverse(camera.matrices.view);
   ubo.projectionMatrixInverse = glm::inverse(camera.matrices.perspective);

   uint8_t* data = (uint8_t*)_sceneUbo->stagingBuffer();
   memcpy(data, &ubo, sizeof(SceneUbo));
   _sceneUbo->syncToGpu(false);
}

void Tutorial::createSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

void Tutorial::setupDescriptorPool()
{
   std::vector<VkDescriptorPoolSize> poolSizes =
   {
      genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
   ,  genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
   };

   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool));
}

void Tutorial::setupDescriptorSetLayout(void)
{
   int bindingIndex = 0;
   std::vector<VkDescriptorSetLayoutBinding> set0Bindings =
   {
      genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   ,  genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++)
   };
   VkDescriptorSetLayoutCreateInfo set0LayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(set0Bindings.data(), static_cast<uint32_t>(set0Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &set0LayoutInfo, nullptr, &_setLayout0));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _setLayout0, _cellManager->cell(0)->layout()->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::VulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   VkPushConstantRange pushConstant{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };
   pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
   pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

   VK_CHECK_RESULT(vkCreatePipelineLayout(_device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
   debugmarker::setName(_device->vulkanDevice(), _pipelineLayout, "_pipelineLayout");
}


void Tutorial::updateDescriptorSet(void)
{
   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &_setLayout0, 1);
   vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &_descriptorSet0);

   int bindingIndex = 0;
   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
   genesis::VulkanInitializers::writeDescriptorSet(_descriptorSet0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindingIndex++,&_sceneUbo->descriptor())
,  genesis::VulkanInitializers::writeDescriptorSet(_descriptorSet0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindingIndex++,&_skyCubeMapTexture->descriptor())
   };

   vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}


void Tutorial::preparePipelines()
{
   // bindings
   std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions
      = { genesis::VulkanInitializers::vertexInputBindingDescription(0, sizeof(genesis::Vertex), VK_VERTEX_INPUT_RATE_VERTEX) };

   // input descriptions
   std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
        genesis::VulkanInitializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, position))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, normal))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(genesis::Vertex, uv))
      , genesis::VulkanInitializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(genesis::Vertex, color))
   };

   // input state
   VkPipelineVertexInputStateCreateInfo vertexInputState
      = genesis::VulkanInitializers::pipelineVertexInputStateCreateInfo(vertexInputBindingDescriptions, vertexInputAttributeDescriptions);

   // input assembly
   VkPipelineInputAssemblyStateCreateInfo inputAssemblyState
      = genesis::VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
         , 0
         , VK_FALSE);

   // viewport state
   VkPipelineViewportStateCreateInfo viewportState =
      genesis::VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);

   // rasterization state
   VkPipelineRasterizationStateCreateInfo rasterizationState =
      genesis::VulkanInitializers::pipelineRasterizationStateCreateInfo(
         VK_POLYGON_MODE_FILL,
         VK_CULL_MODE_BACK_BIT,
         VK_FRONT_FACE_COUNTER_CLOCKWISE,
         0);

   // multisample state
   VkPipelineMultisampleStateCreateInfo multisampleState =
      genesis::VulkanInitializers::pipelineMultisampleStateCreateInfo(
         VK_SAMPLE_COUNT_1_BIT,
         0);

   // depth stencil
   VkPipelineDepthStencilStateCreateInfo depthStencilState =
      genesis::VulkanInitializers::pipelineDepthStencilStateCreateInfo(
         VK_TRUE,
         VK_TRUE,
         VK_COMPARE_OP_LESS_OR_EQUAL);

   // blend attachment
   VkPipelineColorBlendAttachmentState blendAttachmentState =
      genesis::VulkanInitializers::pipelineColorBlendAttachmentState(
         0xf,
         VK_FALSE);

   VkPipelineColorBlendStateCreateInfo colorBlendState =
      genesis::VulkanInitializers::pipelineColorBlendStateCreateInfo(
         1,
         &blendAttachmentState);

   // dynamic states
   std::vector<VkDynamicState> dynamicStateEnables = {
   VK_DYNAMIC_STATE_VIEWPORT,
   VK_DYNAMIC_STATE_SCISSOR
   };
   VkPipelineDynamicStateCreateInfo dynamicState =
      genesis::VulkanInitializers::pipelineDynamicStateCreateInfo(
         dynamicStateEnables.data(),
         static_cast<uint32_t>(dynamicStateEnables.size()),
         0);

   // shader stages
   std::vector<std::pair<std::string, genesis::ShaderType>> shadersToLoad =
   {
        {getShadersPath() + "tutorial/tutorial.vert.spv", genesis::ST_VERTEX_SHADER}
      , {getShadersPath() + "tutorial/tutorial.frag.spv", genesis::ST_FRAGMENT_SHADER}
      , {getShadersPath() + "tutorial/skybox.vert.spv", genesis::ST_VERTEX_SHADER}
      , {getShadersPath() + "tutorial/skybox.frag.spv", genesis::ST_FRAGMENT_SHADER}
   };
   for (auto shaderToLoad : shadersToLoad)
   {
      genesis::Shader* shader = new genesis::Shader(_device);
      shader->loadFromFile(shaderToLoad.first, shaderToLoad.second);
      if (shader->valid() == false)
      {
         std::cout << "error loading shader" << std::endl;
      }
      _shaders.push_back(shader);
   }
   VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = genesis::VulkanInitializers::graphicsPipelineCreateInfo(_pipelineLayout, _renderPass->vulkanRenderPass());

   std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageInfos;
   graphicsPipelineCreateInfo.stageCount = 2;
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();

   // first 2 are the model
   shaderStageInfos[0] = _shaders[0]->pipelineShaderStageCreateInfo();
   shaderStageInfos[1] = _shaders[1]->pipelineShaderStageCreateInfo();
   graphicsPipelineCreateInfo.pVertexInputState = &vertexInputState;
   graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
   graphicsPipelineCreateInfo.pViewportState = &viewportState;
   graphicsPipelineCreateInfo.pRasterizationState = &rasterizationState;
   graphicsPipelineCreateInfo.pMultisampleState = &multisampleState;
   graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilState;
   graphicsPipelineCreateInfo.pColorBlendState = &colorBlendState;
   graphicsPipelineCreateInfo.pDynamicState = &dynamicState;

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_pipeline));

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_pipelineWireframe));
   rasterizationState.polygonMode = VK_POLYGON_MODE_FILL; // reset

   // next 2 are the skybox
   shaderStageInfos[0] = _shaders[2]->pipelineShaderStageCreateInfo();
   shaderStageInfos[1] = _shaders[3]->pipelineShaderStageCreateInfo();

   rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT; // cull the front facing polygons
   depthStencilState.depthWriteEnable = VK_FALSE;
   depthStencilState.depthTestEnable = VK_FALSE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxPipeline));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxPipeline, "_skyBoxPipeline");

   rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_skyBoxPipelineWireframe));
   debugmarker::setName(_device->vulkanDevice(), _skyBoxPipelineWireframe, "_skyBoxPipelineWireframe");
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
      return;
   draw();
}

void Tutorial::viewChanged()
{
   // This function is called by the base example class each time the view is changed by user input
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
   setupDescriptorSetLayout();
   setupDescriptorPool();
   updateDescriptorSet();
   preparePipelines();
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
