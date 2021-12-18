
#include <vulkan/vulkan.h>

#include <windows.h>
#include <processenv.h>

#include "Image.h"
#include "Texture.h"
#include "Buffer.h"
#include "Device.h"
#include "tutorial.h"
#include "Shader.h"
#include "RenderPass.h"
#include "PhysicalDevice.h"

#include "VulkanInitializers.h"
#include "VulkanGltf.h"

#include "GenAssert.h"


#define VENUS 0
#define SPONZA 1

Tutorial::Tutorial()
{
   title = "Vulkan Example - Basic indexed triangle";
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

   enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

   enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
   enabledDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);

   _physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
   _physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
   _physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

   deviceCreatepNextChain = &_physicalDeviceDescriptorIndexingFeatures;
}

Tutorial::~Tutorial()
{
   for (genesis::Shader* shader : _shaders)
   {
      delete shader;
   }
   vkDestroyPipeline(_device->vulkanDevice(), _pipeline, nullptr);

   vkDestroyPipelineLayout(_device->vulkanDevice(), _pipelineLayout, nullptr);

   delete _gltfModel;
   delete _sceneUbo;

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

      // Bind the rendering pipeline
      // The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
      vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet0, 0, nullptr);

      _gltfModel->draw(drawCmdBuffers[i], _pipelineLayout);

      drawUI(drawCmdBuffers[i]);

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
   }

}

void Tutorial::draw()
{
   VulkanExampleBase::prepareFrame();

   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
   VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
   VulkanExampleBase::submitFrame();
}

struct SceneUbo
{
public:
   glm::mat4 viewMatrix;
   glm::mat4 projectionMatrix;
};

void Tutorial::prepareSceneUbo()
{
   _sceneUbo = new genesis::Buffer(_device, genesis::BT_UBO, sizeof(SceneUbo), true);
   updateSceneUbo();
}

void Tutorial::updateSceneUbo(void)
{
   SceneUbo ubo;
   ubo.viewMatrix = camera.matrices.view;
   ubo.projectionMatrix = camera.matrices.perspective;

   uint8_t* data = (uint8_t*)_sceneUbo->stagingBuffer();
   memcpy(data, &ubo, sizeof(SceneUbo));
   _sceneUbo->syncToGpu(false);
}

void Tutorial::setupDescriptorPool()
{
   std::vector<VkDescriptorPoolSize> poolSizes =
   {
      genesis::VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
   };

   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::VulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool));
}

void Tutorial::setupDescriptorSetLayout(void)
{
   std::vector<VkDescriptorSetLayoutBinding> set0Bindings =
   {
      genesis::VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
   };
   VkDescriptorSetLayoutCreateInfo set0LayoutInfo = genesis::VulkanInitializers::descriptorSetLayoutCreateInfo(set0Bindings.data(), static_cast<uint32_t>(set0Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &set0LayoutInfo, nullptr, &_setLayout0));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _setLayout0, _gltfModel->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::VulkanInitializers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   VK_CHECK_RESULT(vkCreatePipelineLayout(_device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
}


void Tutorial::updateDescriptorSet(void)
{
   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::VulkanInitializers::descriptorSetAllocateInfo(descriptorPool, &_setLayout0, 1);
   vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &_descriptorSet0);

   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
   genesis::VulkanInitializers::writeDescriptorSet(_descriptorSet0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&_sceneUbo->descriptor())
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
         VK_CULL_MODE_NONE,
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
   };
   std::vector<genesis::Shader*> shaders;
   std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos;
   for (auto shaderToLoad : shadersToLoad)
   {
      genesis::Shader* shader = new genesis::Shader(_device);
      shader->loadFromFile(shaderToLoad.first, shaderToLoad.second);
      if (shader->valid())
      {
         shaderStageInfos.push_back(shader->shaderStageInfo());
      }
      _shaders.push_back(shader);
   }
   VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = genesis::VulkanInitializers::graphicsPipelineCreateInfo(_pipelineLayout, _renderPass->vulkanRenderPass());

   graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
   graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();

   graphicsPipelineCreateInfo.pVertexInputState = &vertexInputState;
   graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
   graphicsPipelineCreateInfo.pViewportState = &viewportState;
   graphicsPipelineCreateInfo.pRasterizationState = &rasterizationState;
   graphicsPipelineCreateInfo.pMultisampleState = &multisampleState;
   graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilState;
   graphicsPipelineCreateInfo.pColorBlendState = &colorBlendState;
   graphicsPipelineCreateInfo.pDynamicState = &dynamicState;

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(_device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_pipeline));

   for (auto shader : shaders)
   {
      delete shader;
   }
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
   Tutorial::updateSceneUbo();
}

void Tutorial::loadAssets(void)
{
   _gltfModel = new genesis::VulkanGltfModel(_device, true, false);
#if SPONZA
   _gltfModel->loadFromFile(getAssetPath() + "models/sponza/sponza.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif

#if VENUS
   _gltfModel->loadFromFile(getAssetPath() + "models/venus.gltf"
      , genesis::VulkanGltfModel::FlipY | genesis::VulkanGltfModel::PreTransformVertices | genesis::VulkanGltfModel::PreMultiplyVertexColors);
#endif
}

void Tutorial::prepare()
{
   VulkanExampleBase::prepare();
   loadAssets();

   prepareSceneUbo();
   setupDescriptorSetLayout();
   setupDescriptorPool();
   updateDescriptorSet();
   preparePipelines();
   buildCommandBuffers();
   
   prepared = true;
}

void Tutorial::getEnabledFeatures()
{
   // Example uses multi draw indirect if available
   if (_physicalDevice->physicalDeviceFeatures().multiDrawIndirect) {
      _physicalDevice->enabledPhysicalDeviceFeatures().multiDrawIndirect = VK_TRUE;
   }
   // Enable anisotropic filtering if supported
   if (_physicalDevice->physicalDeviceFeatures().samplerAnisotropy) {
      _physicalDevice->enabledPhysicalDeviceFeatures().samplerAnisotropy = VK_TRUE;
   }
};
