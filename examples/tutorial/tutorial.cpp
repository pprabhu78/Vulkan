
#include <vulkan/vulkan.h>

#include <windows.h>
#include <processenv.h>

#include "Image.h"
#include "Texture.h"
#include "Buffer.h"
#include "Device.h"
#include "tutorial.h"
#include "Shader.h"

#include "VulkanInitializers.h"
#include "VulkanGltf.h"

#include "GenAssert.h"


Tutorial::Tutorial()
   : device(nullptr)
{
   title = "Vulkan Example - Basic indexed triangle";
   camera.type = Camera::CameraType::lookat;
   camera.setPosition(glm::vec3(0.0f, 0.0f, -8.5f));
   camera.setRotation(glm::vec3(0.0f));
   camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
}

Tutorial::~Tutorial()
{
   vkDestroyPipeline(device->vulkanDevice(), _pipeline, nullptr);

   vkDestroyPipelineLayout(device->vulkanDevice(), _pipelineLayout, nullptr);

   delete _gltfModel;
   delete _uboScene;

   vkDestroyDescriptorSetLayout(device->vulkanDevice(), _setLayout0, nullptr);
}

void Tutorial::setupRenderPass()
{
   VulkanExampleBase::setupRenderPass();
   if (!device)
   {
      device = new genesis::Device(VulkanExampleBase::device, VulkanExampleBase::queue, VulkanExampleBase::cmdPool, VulkanExampleBase::deviceMemoryProperties);
   }
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

   VkRenderPassBeginInfo renderPassBeginInfo = genesis::vulkanInitalizers::renderPassBeginInfo();
   renderPassBeginInfo.renderPass = renderPass;
   renderPassBeginInfo.renderArea.offset.x = 0;
   renderPassBeginInfo.renderArea.offset.y = 0;
   renderPassBeginInfo.renderArea.extent.width = width;
   renderPassBeginInfo.renderArea.extent.height = height;
   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;

   const VkViewport viewport = genesis::vulkanInitalizers::viewport((float)width, (float)height, 0.0f, 1.0f);
   const VkRect2D scissor = genesis::vulkanInitalizers::rect2D(width, height, 0, 0);

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

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
   }

}

void Tutorial::draw()
{
   // Get next image in the swap chain (back/front buffer)
   VK_CHECK_RESULT(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));

   // Use a fence to wait until the command buffer has finished execution before using it again
   VK_CHECK_RESULT(vkWaitForFences(device->vulkanDevice(), 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX));
   VK_CHECK_RESULT(vkResetFences(device->vulkanDevice(), 1, &waitFences[currentBuffer]));

   // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
   VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   // The submit info structure specifies a command buffer queue submission batch
   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.pWaitDstStageMask = &waitStageMask;               // Pointer to the list of pipeline stages that the semaphore waits will occur at
   submitInfo.pWaitSemaphores = &semaphores.presentComplete;      // Semaphore(s) to wait upon before the submitted command buffer starts executing
   submitInfo.waitSemaphoreCount = 1;                           // One wait semaphore
   submitInfo.pSignalSemaphores = &semaphores.renderComplete;     // Semaphore(s) to be signaled when command buffers have completed
   submitInfo.signalSemaphoreCount = 1;                         // One signal semaphore
   submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer]; // Command buffers(s) to execute in this batch (submission)
   submitInfo.commandBufferCount = 1;                           // One command buffer

   // Submit to the graphics queue passing a wait fence
   VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]));

   // Present the current buffer to the swap chain
   // Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
   // This ensures that the image is not presented to the windowing system until all commands have been submitted
   VkResult present = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
   if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR))) {
      VK_CHECK_RESULT(present);
   }
}

struct ShaderUbo
{
public:
   glm::mat4 viewMatrix;
   glm::mat4 projectionMatrix;
};

void Tutorial::prepareSceneUbo()
{
   _uboScene = new genesis::Buffer(device, genesis::BT_UBO, sizeof(ShaderUbo), true);
   updateUbo();
}

void Tutorial::updateUbo(void)
{
   ShaderUbo ubo;
   ubo.viewMatrix = camera.matrices.view;
   ubo.projectionMatrix = camera.matrices.perspective;

   uint8_t* data = (uint8_t*)_uboScene->stagingBuffer();
   memcpy(data, &ubo, sizeof(ShaderUbo));
   _uboScene->syncToGpu(false);
}

void Tutorial::setupDescriptorPool()
{
   std::vector<VkDescriptorPoolSize> poolSizes =
   {
      genesis::vulkanInitalizers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
   };

   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::vulkanInitalizers::descriptorPoolCreateInfo(poolSizes, 1);
   VK_CHECK_RESULT(vkCreateDescriptorPool(device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool));
}

void Tutorial::setupDescriptorSetLayout(void)
{
   std::vector<VkDescriptorSetLayoutBinding> set0Bindings =
   {
      genesis::vulkanInitalizers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
   };
   VkDescriptorSetLayoutCreateInfo set0LayoutInfo = genesis::vulkanInitalizers::descriptorSetLayoutCreateInfo(set0Bindings.data(), static_cast<uint32_t>(set0Bindings.size()));
   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->vulkanDevice(), &set0LayoutInfo, nullptr, &_setLayout0));

   std::vector<VkDescriptorSetLayout> vecDescriptorSetLayout = { _setLayout0, _gltfModel->vulkanDescriptorSetLayout() };

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = genesis::vulkanInitalizers::pipelineLayoutCreateInfo(vecDescriptorSetLayout.data(), (uint32_t)vecDescriptorSetLayout.size());

   VK_CHECK_RESULT(vkCreatePipelineLayout(device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
}


void Tutorial::updateDescriptorSet(void)
{
   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::vulkanInitalizers::descriptorSetAllocateInfo(descriptorPool, &_setLayout0, 1);
   vkAllocateDescriptorSets(device->vulkanDevice(), &descriptorSetAllocateInfo, &_descriptorSet0);

   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
   genesis::vulkanInitalizers::writeDescriptorSet(_descriptorSet0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&_uboScene->descriptor())
   };

   vkUpdateDescriptorSets(device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
struct Vertex {
   float position[3];
   float normal[3];
   float uv[2];
   float color[3];
};

void Tutorial::preparePipelines()
{
   // bindings
   std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions 
      = { genesis::vulkanInitalizers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX) };
   
   // input descriptions
   std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
        genesis::vulkanInitalizers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position))
      , genesis::vulkanInitalizers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal))
      , genesis::vulkanInitalizers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv))
      , genesis::vulkanInitalizers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color))
   };

   // input state
   VkPipelineVertexInputStateCreateInfo vertexInputState 
      = genesis::vulkanInitalizers::pipelineVertexInputStateCreateInfo(vertexInputBindingDescriptions, vertexInputAttributeDescriptions);

   // input assembly
   VkPipelineInputAssemblyStateCreateInfo inputAssemblyState 
      = genesis::vulkanInitalizers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
         , 0
         , VK_FALSE);

   // viewport state
   VkPipelineViewportStateCreateInfo viewportState =
      genesis::vulkanInitalizers::pipelineViewportStateCreateInfo(1, 1, 0);

   // rasterization state
   VkPipelineRasterizationStateCreateInfo rasterizationState =
      genesis::vulkanInitalizers::pipelineRasterizationStateCreateInfo(
         VK_POLYGON_MODE_FILL,
         VK_CULL_MODE_NONE,
         VK_FRONT_FACE_COUNTER_CLOCKWISE,
         0);

   // multisample state
   VkPipelineMultisampleStateCreateInfo multisampleState =
      genesis::vulkanInitalizers::pipelineMultisampleStateCreateInfo(
         VK_SAMPLE_COUNT_1_BIT,
         0);

   // depth stencil
   VkPipelineDepthStencilStateCreateInfo depthStencilState =
      genesis::vulkanInitalizers::pipelineDepthStencilStateCreateInfo(
         VK_TRUE,
         VK_TRUE,
         VK_COMPARE_OP_LESS_OR_EQUAL);

   // blend attachment
   VkPipelineColorBlendAttachmentState blendAttachmentState =
      genesis::vulkanInitalizers::pipelineColorBlendAttachmentState(
         0xf,
         VK_FALSE);

   VkPipelineColorBlendStateCreateInfo colorBlendState =
      genesis::vulkanInitalizers::pipelineColorBlendStateCreateInfo(
         1,
         &blendAttachmentState);

   // dynamic states
   std::vector<VkDynamicState> dynamicStateEnables = {
   VK_DYNAMIC_STATE_VIEWPORT,
   VK_DYNAMIC_STATE_SCISSOR
   };
   VkPipelineDynamicStateCreateInfo dynamicState =
      genesis::vulkanInitalizers::pipelineDynamicStateCreateInfo(
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
      genesis::Shader* shader = new genesis::Shader(device);
      shader->loadFromFile(shaderToLoad.first, shaderToLoad.second);
      if (shader->valid())
      {
         shaderStageInfos.push_back(shader->shaderStageInfo());
      }
   }
   VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
   graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   graphicsPipelineCreateInfo.pNext = nullptr;
   graphicsPipelineCreateInfo.flags = 0;
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

   graphicsPipelineCreateInfo.layout = _pipelineLayout;
   graphicsPipelineCreateInfo.renderPass = renderPass;

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &_pipeline));

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
   Tutorial::updateUbo();
}

void Tutorial::loadAssets(void)
{
   _gltfModel = new genesis::VulkanGltfModel(device);
   _gltfModel->loadFromFile(getAssetPath() + "models/voyager.gltf", 0);
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



