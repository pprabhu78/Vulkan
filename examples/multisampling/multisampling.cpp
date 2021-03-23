/*
 * Vulkan Example - Multisampling using resolve attachments
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to do multisample anti-aliasing (MSAA) 
 * This involves the creation of so-called resolve attachments with a sample count > 1 to which the scene is rendered to (see createMultisampleTarget)
 * These are then resolved into the actual frame buffer with a sampling pattern applied to determine the final, anti-aliased color
 * The resolve process also requires a specific render pass to be created, overriding the base class default render pass setup
 * In addition to the MSAA pipeline, the sample also creates a MSAA pipeline with sample shading enabled
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool useSampleShading = false;
	
	// The sample count to be used for multi-sampling
	// The actual value is determined in the prepare function based on the supported sample counts of the implementation
	VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

	// Stores all Vulkan objects needed to store a color and depth attachment
	struct MultiSampleTarget {
		struct Color {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
		} color;
		struct Depth {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
		} depth;
	} multisampleTarget;

	vkglTF::Model model;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 lightPos = glm::vec4(5.0f, -5.0f, 5.0f, 1.0f);
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;

	struct Pipelines {
		VkPipeline MSAA;
		VkPipeline MSAASampleShading;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Multisampling";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
		camera.setTranslation(glm::vec3(2.5f, 2.5f, -7.5f));
		settings.overlay = true;
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.MSAA, nullptr);
			vkDestroyPipeline(device, pipelines.MSAASampleShading, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			// Destroy the MSAA targets
			vkDestroyImage(device, multisampleTarget.color.image, nullptr);
			vkDestroyImageView(device, multisampleTarget.color.view, nullptr);
			vkFreeMemory(device, multisampleTarget.color.memory, nullptr);
			vkDestroyImage(device, multisampleTarget.depth.image, nullptr);
			vkDestroyImageView(device, multisampleTarget.depth.view, nullptr);
			vkFreeMemory(device, multisampleTarget.depth.memory, nullptr);
			for (FrameObjects& frame : frameObjects) {
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	virtual void getEnabledFeatures()
	{
		// Enable sample rate shading filtering if supported by the device
		enabledFeatures.sampleRateShading = deviceFeatures.sampleRateShading;
		// Enable anisotropic filtering if supported by the device
		enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	}

	// Creates a multi sample render target for color and depth (image and view) that the multi-sampled scene is rendered to
	void createMultisampleTarget()
	{
		// Check if device supports requested sample count for color and depth frame buffer
		assert((deviceProperties.limits.framebufferColorSampleCounts >= sampleCount) && (deviceProperties.limits.framebufferDepthSampleCounts >= sampleCount));

		// Color target
		VkImageCreateInfo info = vks::initializers::imageCreateInfo();
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = swapChain.colorFormat;
		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = sampleCount;
		// Aside from color attachment, we also add the transient usage flag which may result in better performance or lower memory requirements for some implementations
		info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &multisampleTarget.color.image));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, multisampleTarget.color.image, &memReqs);
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		// We prefer a lazily allocated memory type, this means that the memory gets allocated when the implementation sees fit, e.g. when first using the images
		// This is mostly supported on mobile and results in better performance for tile-based architectures
		VkBool32 lazyMemTypePresent;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &lazyMemTypePresent);
		if (!lazyMemTypePresent) {
			// If this is not available, fall back to device local memory
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &multisampleTarget.color.memory));
		vkBindImageMemory(device, multisampleTarget.color.image, multisampleTarget.color.memory, 0);

		// Create image view for the MSAA target
		VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.image = multisampleTarget.color.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = swapChain.colorFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &multisampleTarget.color.view));

		// Depth target
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = depthFormat;
		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = sampleCount;
		// Image will only be used as a transient target
		info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &multisampleTarget.depth.image));

		vkGetImageMemoryRequirements(device, multisampleTarget.depth.image, &memReqs);
		memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &lazyMemTypePresent);
		if (!lazyMemTypePresent) {
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &multisampleTarget.depth.memory));
		vkBindImageMemory(device, multisampleTarget.depth.image, multisampleTarget.depth.memory, 0);

		// Create image view for the MSAA target
		viewInfo.image = multisampleTarget.depth.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &multisampleTarget.depth.view));
	}

	// Create a render pass for using the multi sampled attachment and resolving it into the frame buffer
	// This overrides the default render pass setup of the example base class
	void setupRenderPass()
	{
		// Setup the attachments used in this render pass
		std::array<VkAttachmentDescription, 3> attachments = {};

		// Multisampled color attachment that the sample will render to
		attachments[0].format = swapChain.colorFormat;
		// We need to set the sample count for this attachment
		attachments[0].samples = sampleCount;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Multisampled depth attachment that the sample will render to
		attachments[1].format = depthFormat;
		// We need to set the sample count for this attachment
		attachments[1].samples = sampleCount;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// This is the frame buffer attachment to which the multisampled image will be resolved to and which will be presented to the swapchain
		attachments[2].format = swapChain.colorFormat;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Setup the attachment references
		// Multisampled attachments
		VkAttachmentReference colorReference{};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference depthReference{};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		// Resolve attachment reference for the color attachment
		VkAttachmentReference resolveReference{};
		resolveReference.attachment = 2;
		resolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Similar to the default render pass, this sample uses one subpass with two dependencies
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		// Pass our resolve attachments to the sub pass
		subpass.pResolveAttachments = &resolveReference;
		subpass.pDepthStencilAttachment = &depthReference;

		std::array<VkSubpassDependency, 2> dependencies;
		
		// Dependency from external (everything before this subpass) will transform the image to the layouts of the references
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Dependency from our single subpass to external (everything after this subpass) will transform the images to their final layouts
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
	}

	// Create the frame buffers for the swap chain images using the msaa and resolve attachments created above
	// This overrides the default frame buffer setup of the example base class
	void setupFrameBuffer()
	{
		createMultisampleTarget();

		// The image views for the attachments must match the references specified at render pass creation
		std::array<VkImageView, 3> attachments;
		attachments[0] = multisampleTarget.color.view;
		attachments[1] = multisampleTarget.depth.view;

		VkFramebufferCreateInfo frameBufferCreateInfo{};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = attachments.size();
		frameBufferCreateInfo.pAttachments = attachments.data();
		frameBufferCreateInfo.width = width;
		frameBufferCreateInfo.height = height;
		frameBufferCreateInfo.layers = 1;
		// Create a frame buffer for every swap chain image
		frameBuffers.resize(swapChain.imageCount);
		for (uint32_t i = 0; i < frameBuffers.size(); i++) {
			attachments[2] = swapChain.buffers[i].view;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
		}
	}

	void loadAssets()
	{
		model.loadFromFile(getAssetPath() + "models/voyager.gltf", vulkanDevice, queue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
	}

	void createDescriptors()
	{
		// Pool
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
			VkWriteDescriptorSet modelWriteDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &modelWriteDescriptorSet, 0, nullptr);
		}
	}

	void createPipelines()
	{
		// Layout 
		// The layout uses set 0 for passing the uniform buffer to the vertex shader and set 1 for passing images to the fragment shader (taken from glTF model)
		const std::vector<VkDescriptorSetLayout> setLayouts = {
			descriptorSetLayout,
			vkglTF::descriptorSetLayoutImage,
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Setup multi sampling
		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		// Number of samples to use for rasterization
		multisampleState.rasterizationSamples = sampleCount;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = shaderStages.size();
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color });

		// MSAA rendering pipeline
		shaderStages[0] = loadShader(getShadersPath() + "multisampling/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "multisampling/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.MSAA));

		if (vulkanDevice->features.sampleRateShading) {
			// Create a MSAA pipeline with sample shading enabled
			// Sample shading enables per-sample shading to avoid shader aliasing and smooth out e.g. high frequency texture maps
			// Note: This will trade performance for are more stable image			
			multisampleState.sampleShadingEnable = VK_TRUE;			
			// Specify the minimum number of unique samples to process for each fragment which, results in a smoother image on high frequency textures
			// While this improves image stability, it also comes with a performance cost
			multisampleState.minSampleShading = 0.25f;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.MSAASampleShading));
		}
	}

	// Returns the maximum (multi) sample count supported by the implementation
	// This would usually be a graphics option in a real-world application that the user can choose from
	VkSampleCountFlagBits getMaxUsableSampleCount()
	{
		// Maximum sample count we can use is determined by the color and depth sample counts, so we set the lower of the two as the upper limit for this sample
		VkSampleCountFlags counts = std::min(deviceProperties.limits.framebufferColorSampleCounts, deviceProperties.limits.framebufferDepthSampleCounts);
		if (counts & VK_SAMPLE_COUNT_64_BIT) {
			return VK_SAMPLE_COUNT_64_BIT; 
		}
		if (counts & VK_SAMPLE_COUNT_32_BIT) {
			return VK_SAMPLE_COUNT_32_BIT; 
		}
		if (counts & VK_SAMPLE_COUNT_16_BIT) {
			return VK_SAMPLE_COUNT_16_BIT; 
		}
		if (counts & VK_SAMPLE_COUNT_8_BIT) {
			return VK_SAMPLE_COUNT_8_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_4_BIT) {
			return VK_SAMPLE_COUNT_4_BIT;
		}
		if (counts & VK_SAMPLE_COUNT_2_BIT) {
			return VK_SAMPLE_COUNT_2_BIT;
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	void prepare()
	{
		sampleCount = getMaxUsableSampleCount();
		if (sampleCount == VK_SAMPLE_COUNT_1_BIT) {
			vks::tools::exitFatal("Multi sampling is not supported", -1);
		}
		// As the UI overlay is rendered as part of the samples' render pass, we need to pass the sample count so the overlay can adjust it's setup accordingly
		UIOverlay.setSampleCount(sampleCount);
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
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform data for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		
		// For each attachment used by this render pass, a clear value has to be specified
		VkClearValue clearValues[3];
		clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		clearValues[2].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };

		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, clearValues, 3);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);

		// Render the model using the selected MSAA pipeline
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, useSampleShading ? pipelines.MSAASampleShading : pipelines.MSAA);
		model.draw(commandBuffer, vkglTF::RenderFlags::BindImages, pipelineLayout);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (vulkanDevice->features.sampleRateShading) {
			if (overlay->header("Settings")) {
				overlay->checkBox("Sample rate shading", &useSampleShading);
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
