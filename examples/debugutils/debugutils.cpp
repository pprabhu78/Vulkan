/*
 * Vulkan Example - Using the VK_EXT_debug_utils extension
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to use the debug utilities to add debugging information to Vulkan objects
 * The debug utilities extension adds functions for e.g. structuring commands into named regions and add ing debug names to objects to be displayed in a graphics debugger
 * It replaces the VK_EXT_debug_marker extension and improves upon it's concepts
 * This makes it easier to structure frame traces and find Vulkan objects in a debugger like RenderDoc (https://renderdoc.org/)
 * The sample implements a basic multi-pass bloom and:
 * - Loads the function pointers for the debug utils naming functions (see prepare)
 * - Names the Vulkan objects used in this sample (see nameDebugObjects), which replaces auto-generated names in the graphics debugger
 * - Adds named and colored debug regions to the command buffer, which is visualized in the scene graph of the graphics debugger (see render)
 * Note: Requires an implementation that supports the VK_EXT_debug_utils extension
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool wireframe = true;
	bool bloom = true;
	bool extensionPresent = false;

	struct Models {
		vkglTF::Model scene;
		vkglTF::Model glowParts;
	} models;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 lightPos = glm::vec4(0.0f, 5.0f, 15.0f, 1.0f);
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;
	// The descriptor set for the offscreen image is static, and not required to be per-frame
	VkDescriptorSet offscreenimageDescriptorSet;

	struct Pipelines {
		VkPipeline toonshading;
		VkPipeline color;
		VkPipeline wireframe = VK_NULL_HANDLE;
		VkPipeline postprocess;
	} pipelines;
	VkPipelineLayout pipelineLayout;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout uniformbuffers;
		VkDescriptorSetLayout offscreenimage;
	} descriptorSetLayouts;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	};
	struct OffscreenPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment color, depth;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
	} offscreenPass;

	// Function pointers for the debug utils extension
	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
	PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
	PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
	PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
	PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
	PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
	PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Debug information with VK_EXT_debug_utils";
		camera.setRotation(glm::vec3(-4.35f, 16.25f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPosition(glm::vec3(0.1f, 1.1f, -8.5f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		settings.overlay = true;
		
		// To use the debug utils, we need to enable it's instance extension
		enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.toonshading, nullptr);
			vkDestroyPipeline(device, pipelines.color, nullptr);
			vkDestroyPipeline(device, pipelines.postprocess, nullptr);
			if (pipelines.wireframe != VK_NULL_HANDLE) {
				vkDestroyPipeline(device, pipelines.wireframe, nullptr);
			}
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.uniformbuffers, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.offscreenimage, nullptr);
			vkDestroyImageView(device, offscreenPass.color.view, nullptr);
			vkDestroyImage(device, offscreenPass.color.image, nullptr);
			vkFreeMemory(device, offscreenPass.color.memory, nullptr);
			vkDestroyImageView(device, offscreenPass.depth.view, nullptr);
			vkDestroyImage(device, offscreenPass.depth.image, nullptr);
			vkFreeMemory(device, offscreenPass.depth.memory, nullptr);
			vkDestroyRenderPass(device, offscreenPass.renderPass, nullptr);
			vkDestroySampler(device, offscreenPass.sampler, nullptr);
			vkDestroyFramebuffer(device, offscreenPass.frameBuffer, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		// Fill mode non solid is required for wireframe display
		enabledFeatures.fillModeNonSolid = deviceFeatures.fillModeNonSolid;
		wireframe = deviceFeatures.fillModeNonSolid;
	}


	// Create all Vulkan objects for for rendering the glowing parts of the scene to an offscreen image used to apply bloom to the scene
	void createOffscreenPassObjects()
	{
		offscreenPass.width = width / 4;
		offscreenPass.height = height / 4;

		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		// Color attachment
		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = VK_FORMAT_R8G8B8A8_UNORM;
		image.extent.width = offscreenPass.width;
		image.extent.height = offscreenPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		// We will sample directly from the color attachment
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &offscreenPass.color.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, offscreenPass.color.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass.color.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, offscreenPass.color.image, offscreenPass.color.memory, 0));

		VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
		colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorImageView.format = VK_FORMAT_R8G8B8A8_UNORM;
		colorImageView.subresourceRange = {};
		colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorImageView.subresourceRange.baseMipLevel = 0;
		colorImageView.subresourceRange.levelCount = 1;
		colorImageView.subresourceRange.baseArrayLayer = 0;
		colorImageView.subresourceRange.layerCount = 1;
		colorImageView.image = offscreenPass.color.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreenPass.color.view));

		// Create sampler to sample from the attachment in the fragment shader
		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &offscreenPass.sampler));
		// Depth stencil attachment
		image.format = fbDepthFormat;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &offscreenPass.depth.image));
		vkGetImageMemoryRequirements(device, offscreenPass.depth.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass.depth.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, offscreenPass.depth.image, offscreenPass.depth.memory, 0));

		VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format = fbDepthFormat;
		depthStencilView.flags = 0;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = 1;
		depthStencilView.image = offscreenPass.depth.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &offscreenPass.depth.view));

		// Create a separate render pass for the offscreen rendering as it may differ from the one used for scene rendering

		std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
		// Color attachment
		attchmentDescriptions[0].format = VK_FORMAT_R8G8B8A8_UNORM;
		attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// Depth attachment
		attchmentDescriptions[1].format = fbDepthFormat;
		attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
		renderPassInfo.pAttachments = attchmentDescriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offscreenPass.renderPass));

		VkImageView attachments[2];
		attachments[0] = offscreenPass.color.view;
		attachments[1] = offscreenPass.depth.view;
		VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = offscreenPass.renderPass;
		fbufCreateInfo.attachmentCount = 2;
		fbufCreateInfo.pAttachments = attachments;
		fbufCreateInfo.width = offscreenPass.width;
		fbufCreateInfo.height = offscreenPass.height;
		fbufCreateInfo.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreenPass.frameBuffer));

		// Fill a descriptor for later use in a descriptor set
		offscreenPass.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		offscreenPass.descriptor.imageView = offscreenPass.color.view;
		offscreenPass.descriptor.sampler = offscreenPass.sampler;

	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.glowParts.loadFromFile(getAssetPath() + "models/treasure_glow.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, getFrameCount() + 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layout for passing matrices
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.uniformbuffers));
		// Layout for passing the offscreen image
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.offscreenimage));

		// Sets
		// Per-Frame uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.uniformbuffers, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// Offscreen image is static, so we need only one global set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.offscreenimage, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &offscreenimageDescriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(offscreenimageDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , 0, &offscreenPass.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	void createPipelines()
	{
		// Layout
		std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.uniformbuffers, descriptorSetLayouts.offscreenimage };
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR	};
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color});

		// Toon shading pipeline
		shaderStages[0] = loadShader(getShadersPath() + "debugutils/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "debugutils/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.toonshading));

		// Color only pipeline
		shaderStages[0] = loadShader(getShadersPath() + "debugutils/colorpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "debugutils/colorpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCI.renderPass = offscreenPass.renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.color));

		// Wire frame rendering pipeline if supported
		if (deviceFeatures.fillModeNonSolid) {
			rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
			pipelineCI.renderPass = renderPass;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe));
		}

		// Post processing bloom effect
		shaderStages[0] = loadShader(getShadersPath() + "debugutils/postprocess.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "debugutils/postprocess.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		depthStencilStateCI.depthTestEnable = VK_FALSE;
		depthStencilStateCI.depthWriteEnable = VK_FALSE;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		blendAttachmentState.colorWriteMask = 0xF;
		blendAttachmentState.blendEnable =  VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.postprocess));
	}

	// All debug utils naming functions use info structures to set the debugging information, so we wrap them for easier access and less boiler-plate

	// Sets the name for a Vulkan handle, the objectType has to match the actual type of the passed Vulkan handle: e.g. VK_OBJECT_TYPE_PIPELINE for a VkPipeline handle
	void setObjectName(VkObjectType objectType, uint64_t objectHandle, const std::string& objectName)
	{
		VkDebugUtilsObjectNameInfoEXT objectNameInfo{};
		objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		objectNameInfo.objectType = objectType;
		objectNameInfo.objectHandle = objectHandle;
		objectNameInfo.pObjectName = objectName.c_str();
		vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
	}

	// These function are based on command buffers

	// Open a new debug label region in the given command buffer. All following commands will be considered part of this label until it's closed with the corresponding end label function.
	void cmdBeginDebugLabel(VkCommandBuffer cmdbuffer, const std::string& labelName, std::vector<float> color)
	{
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		debugLabel.pLabelName = labelName.c_str();
		debugLabel.color[0] = color[0];
		debugLabel.color[1] = color[1];
		debugLabel.color[2] = color[2];
		debugLabel.color[3] = color[3];
		vkCmdBeginDebugUtilsLabelEXT(cmdbuffer, &debugLabel);
	}

	// Insert a single label into the given command buffer
	void cmdInsertDebugLabel(VkCommandBuffer commandBuffer, const std::string& labelName, std::vector<float> color)
	{
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		debugLabel.pLabelName = labelName.c_str();
		debugLabel.color[0] = color[0];
		debugLabel.color[1] = color[1];
		debugLabel.color[2] = color[2];
		debugLabel.color[3] = color[3];
		vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	}

	// Close the current debug label region in the given command buffer
	void cmdEndDebugLabel(VkCommandBuffer cmdBuffer)
	{
		vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
	}

	// These functions are based on a queue

	// Open a new debug label region in the given queue. All following commands will be considered part of this label until it's closed with the corresponding end label function.
	void queueBeginDebugLabel(VkQueue queue, const std::string& labelName, std::vector<float> color)
	{
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		debugLabel.pLabelName = labelName.c_str();
		debugLabel.color[0] = color[0];
		debugLabel.color[1] = color[1];
		debugLabel.color[2] = color[2];
		debugLabel.color[3] = color[3];
		vkQueueBeginDebugUtilsLabelEXT(queue, &debugLabel);
	}

	// Insert a single label into the given queue
	void queueInsterDebugLabel(VkQueue queue, const std::string& labelName, std::vector<float> color)
	{
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		debugLabel.pLabelName = labelName.c_str();
		debugLabel.color[0] = color[0];
		debugLabel.color[1] = color[1];
		debugLabel.color[2] = color[2];
		debugLabel.color[3] = color[3];
		vkQueueInsertDebugUtilsLabelEXT(queue, &debugLabel);
	}

	// Closes the current debug label region in the given queue
	void queueEndDebugLabel(VkQueue queue)
	{
		vkQueueEndDebugUtilsLabelEXT(queue);
	}

	// We will name the Vulkan objects used in this sample
	// These will then show up with those names in a graphics debugging application instead of auto generated names
	// So in RenderDoc e.g. "Pipeline 17" becomes "Toon shading pipeline"
	void nameDebugObjects()
	{
		// Descriptors
		setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayouts.uniformbuffers, "Scene matrices descriptor set layout");
		setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayouts.offscreenimage, "Off-screen images descriptor set layout");
		setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)offscreenimageDescriptorSet, "Off-screen images descriptor");
		for (size_t i = 0; i < frameObjects.size(); i++) {
			setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)frameObjects[i].descriptorSet, "Scene matrices descriptor for frame " + std::to_string(i));
		}
		// Shader modules
		uint32_t moduleIndex = 0;
		if (settings.overlay) {
			setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "User interface vertex shader");
			setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "User interface fragment shader");
		}
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Toon shading vertex shader");
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Toon shading fragment shader");
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Color-only vertex shader");
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Color-only fragment shader");
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Postprocess vertex shader");
		setObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shaderModules[moduleIndex++], "Postprocess fragment shader");
		// Pipelines
		setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipelineLayout, "Shared pipeline layout");
		setObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines.toonshading, "Toon shading pipeline");
		setObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines.color, "Color-only pipeline");
		setObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines.postprocess, "Post processing pipeline");
		if (deviceFeatures.fillModeNonSolid) {
			setObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines.wireframe, "Wireframe rendering pipeline");
		}
		// Images
		setObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)offscreenPass.color.image, "Off-screen color framebuffer");
		setObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)offscreenPass.depth.image, "Off-screen depth framebuffer");
		setObjectName(VK_OBJECT_TYPE_SAMPLER, (uint64_t)offscreenPass.sampler, "Off-screen framebuffer default sampler");
		// Buffers
		setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)models.scene.vertices.buffer, "Scene vertex buffer");
		setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)models.scene.indices.buffer, "Scene index buffer");
		setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)models.glowParts.vertices.buffer, "Glow vertex buffer");
		setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)models.glowParts.indices.buffer, "Glow index buffer");
		for (size_t i = 0; i < frameObjects.size(); i++) {
			setObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)frameObjects[i].commandBuffer, "Command buffer for frame " + std::to_string(i));
			setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)frameObjects[i].uniformBuffer.buffer, "Scene matrices uniform buffer for frame " + std::to_string(i));
			setObjectName(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)frameObjects[i].uniformBuffer.memory, "Scene matrices uniform buffer memory for frame " + std::to_string(i));
		}
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		// Check if the debug utils extension is present
		uint32_t instanceExtensionCount;
		vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
		std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());
		for (VkExtensionProperties& instanceExtension : instanceExtensions) {
			if (strcmp(instanceExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
				extensionPresent = true;
				break;
			}
		}

		// As the debug utils extension is not part of the core, we need ot manually load the function pointers before we can use them
		if (extensionPresent) {
			vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
			vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
			vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
			vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT");
			vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT");
			vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkQueueInsertDebugUtilsLabelEXT");
			vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT");
		} else {
			std::cout << "Warning: " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << " not present, debug utils can't be used.";
		}

		// Prepare per-frame resources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
		}
		loadAssets();
		createOffscreenPassObjects();
		createDescriptors();
		createPipelines();
		nameDebugObjects();
		prepared = true;
	}

	// Add the draw commands for the nodes of a glTF model to the given command buffer and set debug labels for them
	void drawModel(VkCommandBuffer commandBuffer, vkglTF::Model& model)
	{
		model.bindBuffers(commandBuffer);
		for (size_t i = 0; i < model.nodes.size(); i++) {
			// We add labels for each node's draw call into the command buffer so the graphics debugger can display these in the frame trace
			cmdInsertDebugLabel(commandBuffer, "Draw \"" + model.nodes[i]->name + "\"", { 0.0f, 0.0f, 0.0f, 0.0f });
			models.glowParts.drawNode(model.nodes[i], commandBuffer);
		}
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		queueBeginDebugLabel(queue, "Graphics queue command buffer submission for frame " + std::to_string(getCurrentFrameIndex()), { 1.0f, 1.0f, 1.0f, 1.0f });

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform data for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

		// Render the scene using multiple passes, and use cmdBeginDebugLabel and cmEndDebugLabel to put commands into named regions
		// These regions are visualized in the frame trace of the graphics debugger, with the visualization up to the debugging application
		// Usually they are displayed as named and colored groups (as specified in the begin label function)

		// First render pass: Render the glowing parts of the scene to an offscreen buffer that'll be used for the post process blur later on
		if (bloom) {
			const VkViewport viewport = vks::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
			const VkRect2D scissor = vks::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = offscreenPass.renderPass;
			renderPassBeginInfo.framebuffer = offscreenPass.frameBuffer;
			renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
			renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = defaultClearValues;
			// Start a new debug label, all following commands will be part of this label until the next call to endDebugLabel
			cmdBeginDebugLabel(commandBuffer, "Off-screen scene rendering", { 1.0f, 0.78f, 0.05f, 1.0f });
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.color);
			drawModel(commandBuffer, models.glowParts);
			vkCmdEndRenderPass(commandBuffer);
			cmdEndDebugLabel(commandBuffer);
		}

		// Note: Explicit synchronization is not required between the render pass, as this is done implicit via sub pass dependencies

		// Second render pass: Draw the scene and apply a full screen bloom
		VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, defaultClearValues);
		// Start a new debug marker region
		cmdBeginDebugLabel(commandBuffer, "Render scene", { 0.5f, 0.76f, 0.34f, 1.0f });
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// Render the scene with toon shading applied
		// Start a new debug label, all following commands will be part of this label until the next call to endDebugLabel
		if (wireframe) {
			// If wireframe is enabled, we split the screen in half (solid/wireframe)
			renderArea.extent.width = width / 2;
		}
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		cmdBeginDebugLabel(commandBuffer, "Toon shading draw", { 0.78f, 0.74f, 0.9f, 1.0f });
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toonshading);	
		drawModel(commandBuffer, models.scene);
		cmdEndDebugLabel(commandBuffer);

		// Wireframe rendering
		if (wireframe) {
			// Split the screen in half
			renderArea.offset.x = width / 2;
			// Start a new debug label, all following commands will be part of this label until the next call to endDebugLabel
			cmdBeginDebugLabel(commandBuffer, "Wireframe draw", { 0.53f, 0.78f, 0.91f, 1.0f });
			vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
			drawModel(commandBuffer, models.scene);
			cmdEndDebugLabel(commandBuffer);
			// Reset scissor to full render area
			renderArea.offset.x = 0;
			renderArea.extent.width = width;
			vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		}

		// Apply a bloom filter based on the glowing parts of the scene rendered to the offscreen framebuffer
		if (bloom) {
			cmdBeginDebugLabel(commandBuffer, "Apply post processing", { 0.93f, 0.89f, 0.69f, 1.0f });
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &offscreenimageDescriptorSet, 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.postprocess);
			// A full screen triangle is generated by the vertex shaders, so we reuse 3 vertices (for 3 invocations) from current vertex buffer
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			cmdEndDebugLabel(commandBuffer);
		}

		cmdEndDebugLabel(commandBuffer); // "Render scene"

		cmdBeginDebugLabel(commandBuffer, "Render user interface", { 0.0f, 0.6f, 0.6f, 1.0f });
		drawUI(commandBuffer);
		cmdEndDebugLabel(commandBuffer);

		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);

		queueEndDebugLabel(queue);
	}


	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Info")) {
			overlay->text("Important note:");
			overlay->text("Please run this sample from a graphics debugger");
			overlay->text("and do a frame trace to see debug information.");
		}
		if (overlay->header("Settings")) {
			overlay->checkBox("Bloom", &bloom);
			if (deviceFeatures.fillModeNonSolid) {
				overlay->checkBox("Wireframe", &wireframe);
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()