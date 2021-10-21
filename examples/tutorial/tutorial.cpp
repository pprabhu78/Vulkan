
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
   vkDestroyPipeline(device->vulkanDevice(), pipeline, nullptr);

   vkDestroyPipelineLayout(device->vulkanDevice(), pipelineLayout, nullptr);

   delete gltfModel;
   delete uniformBuffer;

   delete texture;
   delete image;

   vkDestroyDescriptorSetLayout(device->vulkanDevice(), descriptorSetLayout, nullptr);
}

void Tutorial::setupRenderPass()
{
   if (!device)
   {
      device = new genesis::Device(VulkanExampleBase::device, VulkanExampleBase::queue, VulkanExampleBase::cmdPool, VulkanExampleBase::deviceMemoryProperties);
   }
   VkRenderPassCreateInfo renderPassCreateInfo = {};

   // color attachment
   std::array< VkAttachmentDescription, 2> attachments = {};
   attachments[0].flags = 0;
   attachments[0].format = swapChain.colorFormat;
   attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
   attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   // depth/stencil
   attachments[1].flags = 0;
   attachments[1].format = depthFormat;
   attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
   attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   // color attachment reference into the attachments array
   VkAttachmentReference colorReference = {};
   colorReference.attachment = 0;
   colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   // depth/stencil attachment reference into the attachments array
   VkAttachmentReference depthStencilReference = {};
   depthStencilReference.attachment = 1;
   depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpassDescription = {};
   subpassDescription.flags = 0;
   subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpassDescription.colorAttachmentCount = 1;
   subpassDescription.pColorAttachments = &colorReference;
   subpassDescription.pDepthStencilAttachment = &depthStencilReference;
   subpassDescription.inputAttachmentCount = 0;
   subpassDescription.pInputAttachments = nullptr;
   subpassDescription.pResolveAttachments = nullptr;
   subpassDescription.preserveAttachmentCount = 0;
   subpassDescription.pPreserveAttachments = nullptr;

   // dependencies
   std::array<VkSubpassDependency, 2> dependencies;
   dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;                             // Producer of the dependency
   dependencies[0].dstSubpass = 0;                                               // Consumer is our single subpass that will wait for the execution dependency
   dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependencies[0].srcAccessMask = 0;
   dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

   dependencies[1].srcSubpass = 0;                             // Producer of the dependency
   dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;           // Consumer is our single subpass that will wait for the execution dependency
   dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   dependencies[1].dstAccessMask = 0;
   dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

   renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassCreateInfo.pNext = nullptr;
   renderPassCreateInfo.flags = 0;
   renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
   renderPassCreateInfo.pAttachments = attachments.data();
   renderPassCreateInfo.subpassCount = 1;
   renderPassCreateInfo.pSubpasses = &subpassDescription;
   renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
   renderPassCreateInfo.pDependencies = dependencies.data();

   VK_CHECK_RESULT(vkCreateRenderPass(device->vulkanDevice(), &renderPassCreateInfo, nullptr, &renderPass));
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

   VkRenderPassBeginInfo renderPassBeginInfo = {};
   renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassBeginInfo.pNext = nullptr;
   renderPassBeginInfo.renderPass = renderPass;
   renderPassBeginInfo.renderArea.offset.x = 0;
   renderPassBeginInfo.renderArea.offset.y = 0;
   renderPassBeginInfo.renderArea.extent.width = width;
   renderPassBeginInfo.renderArea.extent.height = height;
   renderPassBeginInfo.clearValueCount = 2;
   renderPassBeginInfo.pClearValues = clearValues;

   for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
   {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = frameBuffers[i];

      VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

      // Start the first sub pass specified in our default render pass setup by the base class
      // This will clear the color and depth attachment
      vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      // Update dynamic viewport state
      VkViewport viewport = {};
      viewport.height = (float)height;
      viewport.width = (float)width;
      viewport.minDepth = (float)0.0f;
      viewport.maxDepth = (float)1.0f;
      vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

      // Update dynamic scissor state
      VkRect2D scissor = {};
      scissor.extent.width = width;
      scissor.extent.height = height;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

      // Bind descriptor sets describing shader binding points
      vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

      // Bind the rendering pipeline
      // The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
      vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

      // Bind triangle vertex buffer (contains position and colors)
      VkDeviceSize offsets[1] = { 0 };
      VkBuffer buffer = gltfModel->vertexBuffer()->vulkanBuffer();
      vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &buffer, offsets);

      // Bind triangle index buffer
      vkCmdBindIndexBuffer(drawCmdBuffers[i], gltfModel->indexBuffer()->vulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

      // Draw indexed triangle
      vkCmdDrawIndexed(drawCmdBuffers[i], gltfModel->indexBuffer()->sizeInBytes()/sizeof(uint32_t), 1, 0, 0, 1);

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
   glm::mat4 modelViewMatrix;
   glm::mat4 projectionMatrix;
};

void Tutorial::prepareUbo()
{
   uniformBuffer = new genesis::Buffer(device, genesis::BT_UBO, sizeof(ShaderUbo), true);
   updateUbo();
}

void Tutorial::updateUbo(void)
{
   ShaderUbo ubo;
   ubo.modelViewMatrix = camera.matrices.view;
   ubo.projectionMatrix = camera.matrices.perspective;

   uint8_t* data = (uint8_t*)uniformBuffer->stagingBuffer();
   memcpy(data, &ubo, sizeof(ShaderUbo));
   uniformBuffer->syncToGpu(false);
}

void Tutorial::setupDescriptorPool()
{
   VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
   descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   descriptorPoolCreateInfo.maxSets = 1;

   std::vector<VkDescriptorPoolSize> poolSizes =
   {
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
   };

   descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
   descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

   VK_CHECK_RESULT(vkCreateDescriptorPool(device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool));
}

void Tutorial::setupDescriptorSetLayout(void)
{

   std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
   {
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
    , vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
   };

   VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
      vks::initializers::descriptorSetLayoutCreateInfo(
         setLayoutBindings.data(),
         static_cast<uint32_t>(setLayoutBindings.size()));

   VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->vulkanDevice(), &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

   VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
   pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutCreateInfo.pNext = nullptr;
   pipelineLayoutCreateInfo.setLayoutCount = 1;
   pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

   VK_CHECK_RESULT(vkCreatePipelineLayout(device->vulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void Tutorial::setupDescriptorSet(void)
{
   VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
   descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   descriptorSetAllocateInfo.descriptorPool = descriptorPool;
   descriptorSetAllocateInfo.descriptorSetCount = 1;
   descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

   vkAllocateDescriptorSets(device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

   std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
   vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      0,
      &uniformBuffer->descriptor())
   ,
   vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      1,
      &texture->descriptor())
   };

   vkUpdateDescriptorSets(device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
struct Vertex {
   float position[3];
   float normal[3];
   float uv[2];
   float color[3];
};


void Tutorial::prepareTexture(void)
{
   //texture = new Texture(device);
   //texture->loadFromFile(getAssetPath() + "textures/metalplate01_rgba.ktx");

   texture = new genesis::Texture(gltfModel->textures()[1]);

}

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

   graphicsPipelineCreateInfo.layout = pipelineLayout;
   graphicsPipelineCreateInfo.renderPass = renderPass;

   VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->vulkanDevice(), pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline));

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
   gltfModel = new genesis::VulkanGltfModel(device);
   gltfModel->loadFromFile(getAssetPath() + "models/voyager.gltf");
}

void Tutorial::prepare()
{
   VulkanExampleBase::prepare();
   loadAssets();

   prepareUbo();
   prepareTexture();

   setupDescriptorSetLayout();
   preparePipelines();
   setupDescriptorPool();
   setupDescriptorSet();
   buildCommandBuffers();
   
   prepared = true;
}



