/*
 * Vulkan Example - Conditional rendering
 *
 * Copyright (C) 2018-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to use conditional rendering to execute or discard draw commands based the contents of a dedicated buffer
 * A vector stores visibility information for each node in a glTF scene, which can be toggled in the UI, and is uploaded to that buffer
 * During command buffer creation all draw commands for the glTF nodes are then wrapped in vkCmdBegin/EndConditionalRenderingEXT functions 
 * These will discard the draw commands inside that block if the visibility buffer for that node contains 0
 * See createConditionalRenderingBuffer on how to create such a buffer and renderNode for how the conditional functions are used
 * Note: Requires a device that supports the VK_EXT_conditional_rendering extension
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	std::vector<int32_t> conditionalVisibility;
	vks::Buffer conditionalBuffer;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;

	PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
	PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Conditional rendering";
		settings.overlay = true;
		camera.setType(Camera::CameraType::lookat);
		camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(-2.25f, -52.0f, 0.0f));
		camera.setTranslation(glm::vec3(1.9f, 2.05f, -18.0f));
		camera.setRotationSpeed(0.25f);

		// Enable the extension required to use conditional rendering
		enabledDeviceExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		conditionalBuffer.destroy();
		for (FrameObjects& frame : frameObjects) {
			frame.uniformBuffer.destroy();
			destroyBaseFrameObjects(frame);
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/gltf/glTF-Embedded/Buggy.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount());
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSize, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));

		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		// Sets
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		shaderStages[0] = loadShader(getShadersPath() + "conditionalrender/model.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "conditionalrender/model.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	// Copies the contents of our vector for the glTF node's visibility to the conditonal rendering buffer
	void updateConditionalBuffer()
	{
		// We need to wait for the command buffers to finish before we can update the buffer as it may still be used by a frame in flight
		for (FrameObjects& frame : frameObjects) {
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &frame.renderCompleteFence, VK_TRUE, UINT64_MAX));
		}
		memcpy(conditionalBuffer.mapped, conditionalVisibility.data(), sizeof(int32_t) * conditionalVisibility.size());
	}

	// Creates a dedicated buffer to store the visibility information sourced at draw time
	void createConditionalRenderingBuffer()
	{
		// Initialize the vector that stores the visibility information of the scene and which is the source for the conditional buffer
		conditionalVisibility.resize(scene.linearNodes.size());
		for (auto i = 0; i < conditionalVisibility.size(); i++) {
			conditionalVisibility[i] = 1;
		}
		// Conditional values are 32 bits wide and if it's zero the rendering commands are discarded
		// We create a buffer that can hold conditional values for all nodes of the glTF scene
		// The extension introduces the new buffer usage flag VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT that we need to set
		const VkDeviceSize bufferSize = sizeof(int32_t) * conditionalVisibility.size();
		VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(
			VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&conditionalBuffer,
			bufferSize,
			conditionalVisibility.data()));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		// The function pointers for the conditional rendering extensions need to be manually loaded 
		vkCmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)vkGetDeviceProcAddr(device, "vkCmdBeginConditionalRenderingEXT");
		vkCmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)vkGetDeviceProcAddr(device, "vkCmdEndConditionalRenderingEXT");
		if ((!vkCmdEndConditionalRenderingEXT) || (!vkCmdBeginConditionalRenderingEXT)) {
			vks::tools::exitFatal("Could not get the required function pointers", -1);
		}

		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
		}
		loadAssets();
		createConditionalRenderingBuffer();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	// Renders a glTF node recursively using visibility information from the conditional rendering buffer
	void renderNode(vkglTF::Node* node, VkCommandBuffer commandBuffer) {
		if (node->mesh) {
			for (vkglTF::Primitive* primitive : node->mesh->primitives) {
				// A new struct introduced by the extension needs to be filled with information on the conditional buffer
				VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo{};
				conditionalRenderingBeginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
				conditionalRenderingBeginInfo.buffer = conditionalBuffer.buffer;
				// We need to offset into the buffer based on the index of the node to be drawn
				conditionalRenderingBeginInfo.offset = sizeof(int32_t) * node->index;

				// Begin the conditional rendered section
				// If the value from the conditional rendering buffer at the given offset is != 0, the draw commands within this block will be executed, else they will be discarded
				vkCmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingBeginInfo);
				vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
				vkCmdEndConditionalRenderingEXT(commandBuffer);
			}
		};
		// Render all child nodes of this node recursively
		for (auto child : node->children) {
			renderNode(child, commandBuffer);
		}
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform data for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;
		// Scale and flip the view matrix as the model is pretty large
		uniformData.model = scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		VkClearValue clearValues[2];
		clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, clearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// Draw the glTF model
		const VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &scene.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, scene.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Bind the scene matrices to set 0
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		// The meshes in a glTF model are stored in a node-based hierarchy, so we render them starting with the first node
		for (auto node : scene.nodes) {
			renderNode(node, commandBuffer);
		}

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Visibility")) {	
			if (overlay->button("All")) {
				for (auto i = 0; i < conditionalVisibility.size(); i++) {
					conditionalVisibility[i] = 1;
				}
				updateConditionalBuffer();
			}
			ImGui::SameLine();
			if (overlay->button("None")) {
				for (auto i = 0; i < conditionalVisibility.size(); i++) {
					conditionalVisibility[i] = 0;
				}
				updateConditionalBuffer();
			}
			ImGui::NewLine();

			ImGui::BeginChild("InnerRegion", ImVec2(200.0f, 400.0f), false);
			for (auto node : scene.linearNodes) {
				// Add visibility toggle checkboxes for all model nodes with a mesh
				if (node->mesh) {
					if (overlay->checkBox(("[" + std::to_string(node->index) + "] " + node->mesh->name).c_str(), &conditionalVisibility[node->index])) {
						updateConditionalBuffer();
					}
				}
			}
			ImGui::EndChild();
		}
	}

};

VULKAN_EXAMPLE_MAIN()