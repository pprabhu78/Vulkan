/*
 * Vulkan Example - User interfaces with Dear ImGui (https://github.com/ocornut/imgui)
 *
 * Copyright (C) 2017-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * The sample implements a wrapper class for a Dear ImGui based user interface
 * This class contains it's own set of Vulkan resources required for rendering (descriptors, pipelines, buffers)
 * Vertex and index buffers are duplicated per frame, so the sample can have one frame in flight while updating the buffer for the next frame
 */

#include <imgui.h>
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

// Contains options and values to display/toggle in the UI
// Used by both the ImGui wrapper and the example
struct GuiData {
	bool displayModels = true;
	bool displayLogos = true;
	bool displayBackground = true;
	bool animateLight = false;
	float lightSpeed = 0.25f;
	std::array<float, 50> frameTimes{};
	float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
	float lastFrameTime = 0.0f;
	float lightTimer = 0.0f;

	// Updates the frame graph, adding a new frame time entry
	void updateFrameGraph(float frameTime) {
		lastFrameTime = frameTime * 1000.0f;
		std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end());
		frameTimes.back() = lastFrameTime;
		if (lastFrameTime < frameTimeMin) {
			frameTimeMin = lastFrameTime;
		}
		if (lastFrameTime > frameTimeMax) {
			frameTimeMax = lastFrameTime;
		}
	}
} guiData;

// Dear ImGUI wrapper class
// This class wraps all data and functions required to render ImGui in Vulkan
// It uses it's own set of Vulkan objects to decouple it from the example
class ImGuiWrapper {
private:
	VkSampler sampler;
	// Use one set of vertex and index buffers per frame, so we can update data for frame A while frame B is in flight
	struct FrameObjects {
		int32_t vertexCount = 0;
		int32_t indexCount = 0;
		vks::Buffer vertexBuffer;
		vks::Buffer indexBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;
	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	vks::VulkanDevice *device;
	VkPipelineShaderStageCreateInfo vertexShader{};
	VkPipelineShaderStageCreateInfo fragmentShader{};
	// Scale and translation of the user interface are passed to the shader via push constants
	struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;
	uint32_t frameCount = 0;
public:
	ImGuiWrapper()
	{
		ImGui::CreateContext();
		// Set a color scheme
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
	};

	~ImGuiWrapper()
	{
		ImGui::DestroyContext();
		// Release all Vulkan resources required for rendering imGui
		for (FrameObjects& frame : frameObjects) {
			frame.vertexBuffer.destroy();
			frame.indexBuffer.destroy();
		}
		vkDestroyImage(device->logicalDevice, fontImage, nullptr);
		vkDestroyImageView(device->logicalDevice, fontView, nullptr);
		vkFreeMemory(device->logicalDevice, fontMemory, nullptr);
		vkDestroySampler(device->logicalDevice, sampler, nullptr);
		vkDestroyPipelineCache(device->logicalDevice, pipelineCache, nullptr);
		vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
		vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
	}

	// Set the Vulkan device used to create the Vulkan objects (e.g. buffers)
	void setDevice(vks::VulkanDevice* device)
	{
		this->device = device;
	}

	// Set the count of the per-frame objects (same as the render ahead count for the example)
	void setFrameCount(uint32_t frameCount)
	{
		this->frameCount = frameCount;
	}

	// Set the vertex shader to use for UI rendering
	void setVertexShader(VkPipelineShaderStageCreateInfo shader)
	{
		vertexShader = shader;
	}

	// Set the fragment shader to use for UI rendering
	void setFragmentShader(VkPipelineShaderStageCreateInfo shader)
	{
		fragmentShader = shader;
	}

	// Initialize all Vulkan resources used by the UI
	void createOverlayResources(VkRenderPass renderPass, VkQueue copyQueue)
	{
		// Create and upload the font texture provided by ImGui
		// Get the image data for the font atlas that we'll upload to the GPU
		unsigned char* fontData;
		int texWidth, texHeight;
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		VkDeviceSize uploadSize = texWidth*texHeight * 4 * sizeof(char);
		// Create an optimal tiled device local (VRAM) target image to copy the font atlas to
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCI.extent.width = texWidth;
		imageCI.extent.height = texHeight;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &fontImage));
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device->logicalDevice, fontImage, &memReqs);
		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &fontMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, fontImage, fontMemory, 0));
		// Create an image view for the font atlas
		VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
		imageViewCI.image = fontImage;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.layerCount = 1;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &imageViewCI, nullptr, &fontView));

		// Copy the font atlas data to the device local (VRAM) using a staging buffer
		vks::Buffer stagingBuffer;
		VK_CHECK_RESULT(device->createAndMapBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, uploadSize));
		memcpy(stagingBuffer.mapped, fontData, uploadSize);
		// Issue a copy from the staging buffer to the target image
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		// Change image layout for the target image to transfer destination, so we can copy into it
		vks::tools::setImageLayout(copyCmd,
			fontImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);
		// Copy command
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = texWidth;
		bufferCopyRegion.imageExtent.height = texHeight;
		bufferCopyRegion.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(copyCmd,
			stagingBuffer.buffer,
			fontImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion);
		// Change image layout for the target image to shader read, so we can sample it in the fragment shader
		vks::tools::setImageLayout(
			copyCmd,
			fontImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		device->flushCommandBuffer(copyCmd, copyQueue, true);
		stagingBuffer.destroy();

		// Create a sampler for the font atlas image
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &sampler));

		// Create a descriptor pool
		// To keep descriptor setup simple, we duplicate the combined image sampler for the font atlas texture per frame, so we need frameCount descriptors for that type in the pool
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount);
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, frameCount);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Create the descriptor set layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Create the descriptor sets
		frameObjects.resize(frameCount);
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &frame.descriptorSet));
			VkDescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(sampler, fontView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor);
			vkUpdateDescriptorSets(device->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
		}

		// Create a pipeline cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(device->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

		// Create a pipeline layout
		// We use push constants to pass some UI related parameters (scale and translation)
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Create a pipeline
		// Enable blending
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertexShader, fragmentShader };
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		// Vertex bindings and attributes based on ImGui vertex definition
		VkVertexInputBindingDescription vertexInputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX);
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
		pipelineCI.pVertexInputState = &vertexInputState;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	// Starts a new ImGui frame and sets up some windows and UI elements
	void newFrame()
	{
		// Start a new ImGui frame
		ImGui::NewFrame();
		// Add a window with some performance data
		ImGui::SetNextWindowPos(ImVec2(100, 40), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(325, 180), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Performance");
		ImGui::TextUnformatted("Vulkan Example - Dear ImGui");
		ImGui::TextUnformatted(device->properties.deviceName);
		ImGui::Text("%.2f ms/frame", guiData.lastFrameTime);
		ImGui::PlotLines("Frame Times", &guiData.frameTimes[0], 50, 0, "", guiData.frameTimeMin, guiData.frameTimeMax, ImVec2(0, 80));
		ImGui::End();
		// Add another window with some settings
		ImGui::SetNextWindowPos(ImVec2(100, 300), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280, 150), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Example settings");
		ImGui::Checkbox("Render models", &guiData.displayModels);
		ImGui::Checkbox("Display logos", &guiData.displayLogos);
		ImGui::Checkbox("Display background", &guiData.displayBackground);
		ImGui::Checkbox("Animate light", &guiData.animateLight);
		ImGui::SliderFloat("Light speed", &guiData.lightSpeed, 0.1f, 1.0f);
		ImGui::End();
		// Add ImGui's example window
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
		ImGui::ShowDemoWindow();
		// The ImGui::Render method will generate the draw command lists we need to fill the index and vertex buffers
		ImGui::Render();
	}

	// Update vertex and index buffer containing the ImGui elements when required
	void updateBuffers(uint32_t frameIndex)
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();

		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);	
		FrameObjects& currentFrame = frameObjects[frameIndex];
		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
			currentFrame.vertexCount = 0;
			currentFrame.indexCount = 0;
			return;
		}

		// Update buffers only if it's either empty or too small to hold the required number of indices or vertices

		// Vertex buffer
		if ((currentFrame.vertexBuffer.buffer == VK_NULL_HANDLE) || (imDrawData->TotalVtxCount > currentFrame.vertexCount)) {
			currentFrame.vertexBuffer.destroy();
			VK_CHECK_RESULT(device->createAndMapBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &currentFrame.vertexBuffer, vertexBufferSize));
			currentFrame.vertexCount = imDrawData->TotalVtxCount;
		}

		// Index buffer
		if ((currentFrame.indexBuffer.buffer == VK_NULL_HANDLE) || (imDrawData->TotalIdxCount > currentFrame.indexCount)) {
			currentFrame.indexBuffer.destroy();
			VK_CHECK_RESULT(device->createAndMapBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &currentFrame.indexBuffer, indexBufferSize));
			currentFrame.indexCount = imDrawData->TotalIdxCount;
		}

		// Upload data
		ImDrawVert* vtxDst = (ImDrawVert*)currentFrame.vertexBuffer.mapped;
		ImDrawIdx* idxDst = (ImDrawIdx*)currentFrame.indexBuffer.mapped;
		for (int n = 0; n < imDrawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = imDrawData->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}

		// As we don't require a host coherent memory type, flushes are required to make writes visible to the GPU
		currentFrame.vertexBuffer.flush();
		currentFrame.indexBuffer.flush();
	}

	// Draw current IimGui frame into a command buffer
	void drawFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex)
	{
		ImGuiIO& io = ImGui::GetIO();

		FrameObjects currentFrame = frameObjects[frameIndex];

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = vks::initializers::viewport(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// UI scale and translate are passed via push constants
		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

		// Issue the render commands sourced from ImGui's current frame's command lists
		ImDrawData* imDrawData = ImGui::GetDrawData();
		int32_t vertexOffset = 0;
		int32_t indexOffset = 0;

		if (currentFrame.indexCount > 0) {

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &currentFrame.vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, currentFrame.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

			for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
			{
				const ImDrawList* cmd_list = imDrawData->CmdLists[i];
				for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
					VkRect2D scissorRect;
					scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
					scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
					scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
					scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
					vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}
				vertexOffset += cmd_list->VtxBuffer.Size;
			}
		}
	}

};

// Example class
class VulkanExample : public VulkanExampleBase
{
public:
	ImGuiWrapper *imGuiWrapper = nullptr;

	struct Models {
		vkglTF::Model models;
		vkglTF::Model logos;
		vkglTF::Model background;
	} models;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 lightPos;
	};

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkan Example - Dear ImGui";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 0.0f, -4.8f));
		camera.setRotation(glm::vec3(4.5f, -380.0f, 0.0f));
		camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
			delete imGuiWrapper;
		}
	}

	void createDescriptors()
	{
		// Pool
		// One uniform buffer for the scene matrices per frame
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount());
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

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
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0,	VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });;

		shaderStages[0] = loadShader(getShadersPath() + "imgui/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "imgui/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.models.loadFromFile(getAssetPath() + "models/vulkanscenemodels.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.background.loadFromFile(getAssetPath() + "models/vulkanscenebackground.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.logos.loadFromFile(getAssetPath() + "models/vulkanscenelogos.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	// [POI] Create the Dear ImGui wrapper class that renders the UI
	void createImGuiWrapper()
	{
		imGuiWrapper = new ImGuiWrapper();
		imGuiWrapper->setFrameCount(getFrameCount());
		imGuiWrapper->setDevice(vulkanDevice);
		imGuiWrapper->setVertexShader(loadShader(getShadersPath() + "imgui/ui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		imGuiWrapper->setFragmentShader(loadShader(getShadersPath() + "imgui/ui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		imGuiWrapper->createOverlayResources(renderPass, queue);
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame resources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
		}
		loadAssets();
		createDescriptors();
		createPipelines();
		createImGuiWrapper();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// [POI] Update ImGui and the wrapper
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2((float)width, (float)height);
		io.MousePos = ImVec2(mousePos.x, mousePos.y);
		io.MouseDown[0] = mouseButtons.left;
		io.MouseDown[1] = mouseButtons.right;
		// Update the frame graph each second
		if ((frameCounter == 0) && (frameTimer != 1.0f)) {
			guiData.updateFrameGraph(frameTimer);
		}
		imGuiWrapper->newFrame();
		imGuiWrapper->updateBuffers(getCurrentFrameIndex());

		// Update uniform-buffers for the next frame
		UniformData uniformData{};
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelview = camera.matrices.view * glm::mat4(1.0f);
		// Animate the light source
		if (guiData.animateLight) {
			guiData.lightTimer += frameTimer * guiData.lightSpeed;
			uniformData.lightPos.x = sin(glm::radians(guiData.lightTimer * 360.0f)) * 15.0f;
			uniformData.lightPos.z = cos(glm::radians(guiData.lightTimer * 360.0f)) * 15.0f;
		};
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// Models can be toggled on and off via the UI
		if (guiData.displayBackground) {
			models.background.draw(commandBuffer);
		}
		if (guiData.displayModels) {
			models.models.draw(commandBuffer);
		}
		if (guiData.displayLogos) {
			models.logos.draw(commandBuffer);
		}

		// [POI] Render the ImGui
		imGuiWrapper->drawFrame(commandBuffer, getCurrentFrameIndex());

		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void mouseMoved(double x, double y, bool &handled)
	{
		ImGuiIO& io = ImGui::GetIO();
		handled = io.WantCaptureMouse;
	}

};

VULKAN_EXAMPLE_MAIN()
