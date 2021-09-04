/*
 * Vulkan Example - Multiview (VK_KHR_multiview)
 *
 * Copyright (C) 2018-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to use the VK_KHR_multiview for simultaneously rendering to multiple views in a single render pass to simulate stereoscopic rendering
 * For this, a layered image attachment is created (see createMultiviewResources) with two layers 
 * The VkRenderPassMultiviewCreateInfo for the multi view render pass (see createPipelines) causes the shaders to be invoked multiple times, writing to the layers in the image
 * The vertex shader then uses gl_ViewIndex to get the index of the current view for selecting different matrices for the left and right eye
 * A final full screen pass then displays the result of the multi view render pass
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	VkPhysicalDeviceMultiviewFeatures enabledMultiviewFeatures{};

	struct MultiviewPass {
		struct FrameBufferAttachment {
			VkImage image;
			VkDeviceMemory memory;
			VkImageView view;
		} color, depth;
		VkFramebuffer frameBuffer;
		VkRenderPass renderPass;
		VkDescriptorImageInfo descriptor;
		VkSampler sampler;
	} multiviewPass{};

	vkglTF::Model scene;

	struct UniformData {
		glm::mat4 projection[2];
		glm::mat4 modelview[2];
		glm::vec4 lightPos = glm::vec4(-2.5f, -3.5f, 0.0f, 1.0f);
		float distortionAlpha = 0.2f;
	} uniformData;
	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;
	// The descriptor set multiview image is static, and not required to be per-frame
	VkDescriptorSet multiviewImageDescriptorSet;

	struct Pipelines {
		VkPipeline multiviewGeneration;
		VkPipeline multiviewDisplay;
	} pipelines;
	VkPipelineLayout pipelineLayout;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout uniformbuffers;
		VkDescriptorSetLayout multiviewimage;
	} descriptorSetLayouts;

	// Camera and view properties
	float eyeSeparation = 0.08f;
	const float focalLength = 0.5f;
	const float fov = 90.0f;
	const float zNear = 0.1f;
	const float zFar = 256.0f;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Multiview rendering";
		camera.type = Camera::CameraType::firstperson;
		camera.setRotation(glm::vec3(0.0f, 90.0f, 0.0f));
		camera.setTranslation(glm::vec3(7.0f, 3.2f, 0.0f));
		camera.movementSpeed = 5.0f;
		settings.overlay = true;

		// Enable extension required for multiview
		enabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

		// Reading device properties and features for multiview requires VK_KHR_get_physical_device_properties2 to be enabled
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipelines.multiviewGeneration, nullptr);
		vkDestroyPipeline(device, pipelines.multiviewDisplay, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.uniformbuffers, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.multiviewimage, nullptr);
		for (FrameObjects& frame : frameObjects) {
			frame.uniformBuffer.destroy();
			destroyBaseFrameObjects(frame);
		}
		// Multiview pass
		vkDestroyImageView(device, multiviewPass.color.view, nullptr);
		vkDestroyImage(device, multiviewPass.color.image, nullptr);
		vkFreeMemory(device, multiviewPass.color.memory, nullptr);
		vkDestroyImageView(device, multiviewPass.depth.view, nullptr);
		vkDestroyImage(device, multiviewPass.depth.image, nullptr);
		vkFreeMemory(device, multiviewPass.depth.memory, nullptr);
		vkDestroyRenderPass(device, multiviewPass.renderPass, nullptr);
		vkDestroySampler(device, multiviewPass.sampler, nullptr);
		vkDestroyFramebuffer(device, multiviewPass.frameBuffer, nullptr);
	}

	/*
		Prepares all resources required for the layered multiview attachment
		Images, views, attachments, renderpass, framebuffer, etc.
	*/
	void createMultiviewResources()
	{
		// Release resources if the attachments are to be recreated
		if (multiviewPass.color.image != VK_NULL_HANDLE) {
			vkDestroyImageView(device, multiviewPass.color.view, nullptr);
			vkDestroyImage(device, multiviewPass.color.image, nullptr);
			vkFreeMemory(device, multiviewPass.color.memory, nullptr);
			multiviewPass.color = {};
		}
		if (multiviewPass.depth.image != VK_NULL_HANDLE) {
			vkDestroyImageView(device, multiviewPass.depth.view, nullptr);
			vkDestroyImage(device, multiviewPass.depth.image, nullptr);
			vkFreeMemory(device, multiviewPass.depth.memory, nullptr);
			multiviewPass.depth = {};
		}

		// Example renders to two views (left/right)
		const uint32_t multiviewLayerCount = 2;

		// Create a layered depth/stencil image and view
		{
			VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = depthFormat;
			imageCI.extent = { width, height, 1 };
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = multiviewLayerCount;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			imageCI.flags = 0;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &multiviewPass.depth.image));
			VkMemoryRequirements memoryRequirements;
			vkGetImageMemoryRequirements(device, multiviewPass.depth.image, &memoryRequirements);
			VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
			memAllocInfo.allocationSize = memoryRequirements.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &multiviewPass.depth.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, multiviewPass.depth.image, multiviewPass.depth.memory, 0));
			
			VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			imageViewCI.format = depthFormat;
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageViewCI.subresourceRange.baseMipLevel = 0;
			imageViewCI.subresourceRange.levelCount = 1;
			imageViewCI.subresourceRange.baseArrayLayer = 0;
			imageViewCI.subresourceRange.layerCount = multiviewLayerCount;
			imageViewCI.image = multiviewPass.depth.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &multiviewPass.depth.view));
		}

		// Create a layered color image and view
		{
			VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = swapChain.colorFormat;
			imageCI.extent = { width, height, 1 };
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = multiviewLayerCount;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &multiviewPass.color.image));
			VkMemoryRequirements memoryRequirements;
			vkGetImageMemoryRequirements(device, multiviewPass.color.image, &memoryRequirements);
			VkMemoryAllocateInfo memoryAllocInfo = vks::initializers::memoryAllocateInfo();
			memoryAllocInfo.allocationSize = memoryRequirements.size;
			memoryAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &multiviewPass.color.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, multiviewPass.color.image, multiviewPass.color.memory, 0));

			VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			imageViewCI.format = swapChain.colorFormat;
			imageViewCI.flags = 0;
			imageViewCI.subresourceRange = {};
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCI.subresourceRange.baseMipLevel = 0;
			imageViewCI.subresourceRange.levelCount = 1;
			imageViewCI.subresourceRange.baseArrayLayer = 0;
			imageViewCI.subresourceRange.layerCount = multiviewLayerCount;
			imageViewCI.image = multiviewPass.color.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &multiviewPass.color.view));
		}

		// Create sampler to sample from the color image in the fragment shader (if not already created, on resize there is no need to recreate)
		if (multiviewPass.sampler == VK_NULL_HANDLE) {
			VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
			samplerCI.magFilter = VK_FILTER_NEAREST;
			samplerCI.minFilter = VK_FILTER_NEAREST;
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = samplerCI.addressModeU;
			samplerCI.addressModeW = samplerCI.addressModeU;
			samplerCI.mipLodBias = 0.0f;
			samplerCI.maxAnisotropy = 1.0f;
			samplerCI.minLod = 0.0f;
			samplerCI.maxLod = 1.0f;
			samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &multiviewPass.sampler));
		}

		// Fill a descriptor for later use in a descriptor set
		multiviewPass.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		multiviewPass.descriptor.imageView = multiviewPass.color.view;
		multiviewPass.descriptor.sampler = multiviewPass.sampler;

		// Create a render pass (if not already created, on resize there is no need to recreate)
		if (multiviewPass.renderPass == VK_NULL_HANDLE) {
			std::array<VkAttachmentDescription, 2> attachments = {};
			// Color attachment
			attachments[0].format = swapChain.colorFormat;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			// Depth attachment
			attachments[1].format = depthFormat;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference = {};
			colorReference.attachment = 0;
			colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthReference = {};
			depthReference.attachment = 1;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
			subpassDescription.pDepthStencilAttachment = &depthReference;

			// Subpass dependencies for layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassCI{};
			renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCI.attachmentCount = static_cast<uint32_t>(attachments.size());
			renderPassCI.pAttachments = attachments.data();
			renderPassCI.subpassCount = 1;
			renderPassCI.pSubpasses = &subpassDescription;
			renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());
			renderPassCI.pDependencies = dependencies.data();

			// Set up the multivew properties of the renderpass

			// The view mask is a bit mask that specifies which view rendering is broadcast to
			// 0011 = Broadcast to first and second view layer
			const uint32_t viewMask = 0b00000011;

			// The correlation mask is a bit mask that specifies correlation between the views in the renderpass
			// An implementation may use this for optimizations (e.g. concurrent render)
			// As with the view mask 0011 = First and second layer
			const uint32_t correlationMask = 0b00000011;

			VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
			renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
			renderPassMultiviewCI.pViewMasks = &viewMask;
			renderPassMultiviewCI.correlationMaskCount = 1;
			renderPassMultiviewCI.pCorrelationMasks = &correlationMask;
			renderPassMultiviewCI.subpassCount = 1;

			// Chain the extened renderpass create info into the renderpass create info
			renderPassCI.pNext = &renderPassMultiviewCI;
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &multiviewPass.renderPass));
		}

		// Create a framebuffer for the attachments
		if (multiviewPass.frameBuffer != VK_NULL_HANDLE) {
			// Release the old framebuffer if it's to be recreated
			vkDestroyFramebuffer(device, multiviewPass.frameBuffer, nullptr);
		}

		VkImageView attachments[2];
		attachments[0] = multiviewPass.color.view;
		attachments[1] = multiviewPass.depth.view;
		VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
		framebufferCI.renderPass = multiviewPass.renderPass;
		framebufferCI.attachmentCount = 2;
		framebufferCI.pAttachments = attachments;
		framebufferCI.width = width;
		framebufferCI.height = height;
		framebufferCI.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCI, nullptr, &multiviewPass.frameBuffer));
	}

	void loadAssets()
	{
		scene.loadFromFile(getAssetPath() + "models/sampleroom.gltf", vulkanDevice, queue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1 + getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layouts
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
		// One layout for the per-frame uniform buffers
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.uniformbuffers));
		// One layout for multiview image
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.multiviewimage));

		// Descriptors
		// Per-frame uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.uniformbuffers, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// Global set for the multiview image
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.multiviewimage, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &multiviewImageDescriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(multiviewImageDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &multiviewPass.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

	}

	void createPipelines()
	{
		// Layouts
		const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.uniformbuffers, descriptorSetLayouts.multiviewimage };
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI =	vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo();
		pipelineCI.layout = pipelineLayout;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.pVertexInputState  = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

		// Pipeline for rendering to the layered multiview image
		pipelineCI.renderPass = multiviewPass.renderPass;
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		shaderStages[0] = loadShader(getShadersPath() + "multiview/multiview.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "multiview/multiview.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.multiviewGeneration));

		// Pipeline for displaying the layered multiview image
		// Layer selection is done at draw time via the instance index (see render());
		rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCI.pVertexInputState = &emptyInputState;
		pipelineCI.layout = pipelineLayout;
		pipelineCI.renderPass = renderPass;
		shaderStages[0] = loadShader(getShadersPath() + "multiview/viewdisplay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "multiview/viewdisplay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.multiviewDisplay));

	}

	// Updates the two matrices used for the eyes' viewports
	void updateMultiviewMatrices()
	{
		// Matrices for the two viewports
		// See http://paulbourke.net/stereographics/stereorender/

		float aspectRatio = (float)(width * 0.5f) / (float)height;
		float wd2 = zNear * tan(glm::radians(fov / 2.0f));
		float ndfl = zNear / focalLength;
		float left, right;
		float top = wd2;
		float bottom = -wd2;

		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(camera.rotation.x)) * sin(glm::radians(camera.rotation.y));
		camFront.y = sin(glm::radians(camera.rotation.x));
		camFront.z = cos(glm::radians(camera.rotation.x)) * cos(glm::radians(camera.rotation.y));
		camFront = glm::normalize(camFront);
		glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::mat4 rotM = glm::mat4(1.0f);
		glm::mat4 translation;

		rotM = glm::rotate(rotM, glm::radians(camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		// Left eye
		left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
		translation = glm::translate(glm::mat4(1.0f), camera.position - camRight * (eyeSeparation / 2.0f));
		uniformData.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uniformData.modelview[0] = rotM * translation;

		// Right eye
		left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		translation = glm::translate(glm::mat4(1.0f), camera.position + camRight * (eyeSeparation / 2.0f));
		uniformData.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uniformData.modelview[1] = rotM * translation;
	}

	void getEnabledFeatures()
	{
		// Enable the multiview feature using the dedicated physical device structure
		enabledMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;
		enabledMultiviewFeatures.multiview = VK_TRUE;
		deviceCreatepNextChain = &enabledMultiviewFeatures;
	}

	void prepare()
	{
		// Display multi view features and properties in the console
		VkPhysicalDeviceFeatures2KHR deviceFeatures2{};
		VkPhysicalDeviceMultiviewFeaturesKHR extFeatures{};
		extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
		deviceFeatures2.pNext = &extFeatures;
		PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
		vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &deviceFeatures2);
		std::cout << "Multiview features:" << std::endl;
		std::cout << "\tmultiview = " << extFeatures.multiview << std::endl;
		std::cout << "\tmultiviewGeometryShader = " << extFeatures.multiviewGeometryShader << std::endl;
		std::cout << "\tmultiviewTessellationShader = " << extFeatures.multiviewTessellationShader << std::endl;
		std::cout << std::endl;
		VkPhysicalDeviceProperties2KHR deviceProps2{};
		VkPhysicalDeviceMultiviewPropertiesKHR extProps{};
		extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR;
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		deviceProps2.pNext = &extProps;
		PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
		vkGetPhysicalDeviceProperties2KHR(physicalDevice, &deviceProps2);
		std::cout << "Multiview properties:" << std::endl;
		std::cout << "\tmaxMultiviewViewCount = " << extProps.maxMultiviewViewCount << std::endl;
		std::cout << "\tmaxMultiviewInstanceIndex = " << extProps.maxMultiviewInstanceIndex << std::endl;

		VulkanExampleBase::prepare();
		// Prepare per-frame resources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
		}
		loadAssets();
		createMultiviewResources();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		// If the window has been resized, we need to recreate the multiview objects
		if (resized)
		{
			vkDeviceWaitIdle(device);
			createMultiviewResources();
			// As the image has been recreated, we also need to update the descriptor pointing to that image
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(multiviewImageDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &multiviewPass.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update uniform-buffers for the next frame
		updateMultiviewMatrices();
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(UniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		VkRect2D renderArea = getRenderArea();
		VkViewport viewport = getViewport();
		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

		// Bind the uniform buffers to set 0
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		// Bind the multiview image to set 1
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &multiviewImageDescriptorSet, 0, nullptr);

		// Update the layered multiview image attachment with the scene from two different viewpors
		{
			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = multiviewPass.renderPass;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.framebuffer = multiviewPass.frameBuffer;

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.multiviewGeneration);
			scene.draw(commandBuffer);
			vkCmdEndRenderPass(commandBuffer);
		}

		// Display the multiview images
		{
			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.multiviewDisplay);		

			// Left eye
			viewport.width = width * 0.5f;
			renderArea.extent.width = width / 2;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
			// The shaders use the gl_InstanceIndex to select the first image layer (last parameter of vkCmdDraw = 0)
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);

			// Right eye
			viewport.x = (float)width / 2;
			renderArea.offset.x = width / 2;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
			// The shaders use the gl_InstanceIndex to select the second image layer (last parameter of vkCmdDraw = 1)
			vkCmdDraw(commandBuffer, 3, 1, 0, 1);

			drawUI(commandBuffer);

			vkCmdEndRenderPass(commandBuffer);
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->sliderFloat("Eye separation", &eyeSeparation, -1.0f, 1.0f);
			overlay->sliderFloat("Barrel distortion", &uniformData.distortionAlpha, -0.6f, 0.6f);
		}
	}

};

VULKAN_EXAMPLE_MAIN()