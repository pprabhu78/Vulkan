/*
 * Vulkan Example - Text overlay rendering on-top of an existing scene using a separate render pass
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 /*
 * This sample shows how to render a basic 2D text overlay on top of a 3D scene
 * The text overlay uses a font atlas uploaded to a Vulkan image from an stb (https://github.com/nothings/stb) font file
 * The text to be rendered are stored in a vertex buffer containing the single characters to be drawn
 * This is sourced for drawing the overlay on top of a 3D scene
 */

#include <sstream>
#include <iomanip>
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "../external/stb/stb_font_consolas_24_latin1.inl"

#define ENABLE_VALIDATION false

// Vulkan example class
class VulkanExample : public VulkanExampleBase
{
public:
	bool showOverlay = true;

	// Font data from the selected stb font
	stb_fontchar stbFontData[STB_FONT_consolas_24_latin1_NUM_CHARS];
	enum TextAlign { alignLeft, alignCenter, alignRight };
	// Max. number of chars the text overlay buffer can hold
	const uint32_t overlayMaxCharacterCount = 2048;
	// Number of vertices in the current text overlay buffer
	uint32_t overlayVertexCount;
	// Store the image for the font atlas containing ASCII characters
	struct FontAtlas {
		VkSampler sampler;
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
	} fontAtlas;

	vkglTF::Model model;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		// Contains the vertices with the characters for the text overlay
		vks::Buffer textBuffer;
		VkDescriptorSet descriptorSet;
	};
	std::vector<FrameObjects> frameObjects;
	// The descriptor set for the font atlas is static, and not required to be per-frame
	VkDescriptorSet fontAtlasDescriptorSet;

	struct Pipelines {
		VkPipeline model;
		VkPipeline text;
	} pipelines;
	struct PipelineLayouts {
		VkPipelineLayout model;
		VkPipelineLayout text;
	} pipelineLayouts;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout uniformbuffers;
		VkDescriptorSetLayout text;
	} descriptorSetLayouts;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkan Example - Text overlay";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
		camera.setRotation(glm::vec3(-25.0f, -0.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		// Don't use the built-in ImGui UI overlay
		settings.overlay = false;
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipelines.model, nullptr);
		vkDestroyPipeline(device, pipelines.text, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.model, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.text, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.uniformbuffers , nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.text, nullptr);
		vkDestroySampler(device, fontAtlas.sampler, nullptr);
		vkDestroyImage(device, fontAtlas.image, nullptr);
		vkDestroyImageView(device, fontAtlas.view, nullptr);
		vkFreeMemory(device, fontAtlas.memory, nullptr);
		for (FrameObjects& frame : frameObjects) {
			frame.uniformBuffer.destroy();
			frame.textBuffer.destroy();
			destroyBaseFrameObjects(frame);
		}
	}

	// Create the Vulkan objects required to store and draw the text overlay
	void createOverlayResources()
	{
		// Font image setup

		// Load the stb front data into a buffer that we'll upload to the GPU as an image
		const uint32_t fontWidth = STB_FONT_consolas_24_latin1_BITMAP_WIDTH;
		const uint32_t fontHeight = STB_FONT_consolas_24_latin1_BITMAP_WIDTH;
		static unsigned char font24pixels[fontWidth][fontHeight];
		stb_font_consolas_24_latin1(stbFontData, font24pixels, fontHeight);

		// Create a device local optimal tiled target image that the stb font data will be copied to
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R8_UNORM;
		imageCI.extent.width = fontWidth;
		imageCI.extent.height = fontHeight;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &fontAtlas.image));
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, fontAtlas.image, &memoryRequirements);
		VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &fontAtlas.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, fontAtlas.image, fontAtlas.memory, 0));

		// Create an image view for the font atlas
		VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
		imageViewCI.image = fontAtlas.image;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.format = VK_FORMAT_R8_UNORM;
		imageViewCI.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,	VK_COMPONENT_SWIZZLE_A };
		imageViewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCI, nullptr, &fontAtlas.view));

		// Copy the stb font atlas data to the device local (VRAM) using a staging buffer
		// Size of the font texture is WIDTH * HEIGHT (only one color channel)
		VkDeviceSize uploadSize = fontWidth * fontHeight;
		vks::Buffer stagingBuffer;
		VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, uploadSize));
		memcpy(stagingBuffer.mapped, &font24pixels[0][0], uploadSize);
		// Issue a copy from the staging buffer to the target image
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		// Change image layout for the target image to transfer destination, so we can copy into it
		vks::tools::setImageLayout(copyCmd,
			fontAtlas.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);
		// Copy command
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = fontWidth;
		bufferCopyRegion.imageExtent.height = fontHeight;
		bufferCopyRegion.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(copyCmd,
			stagingBuffer.buffer,
			fontAtlas.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion);
		// Change image layout for the target image to shader read, so we can sample it in the fragment shader
		vks::tools::setImageLayout(
			copyCmd,
			fontAtlas.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
		stagingBuffer.destroy();

		// Create a sampler for the font atlas image
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.mipLodBias = 0.0f;
		samplerCI.compareOp = VK_COMPARE_OP_NEVER;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &samplerCI, nullptr, &fontAtlas.sampler));

		// Create per-frame vertex buffers containing the character data for the overlay text
		// These are per-frame, so we can update the buffer for the next frame while the previous frame is still being processed
		// Note: For simplicity, buffers are created with a fixed maximum size
		for (FrameObjects& frame : frameObjects) {
			VkDeviceSize vertexBufferSize = overlayMaxCharacterCount * sizeof(glm::vec4);
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &frame.textBuffer, vertexBufferSize));
		}
	}

	// Add a string to the current buffer at the given position
	void addTextToBuffer(glm::vec4** target, std::string text, float x, float y, TextAlign align)
	{
		const uint32_t firstChar = STB_FONT_consolas_24_latin1_FIRST_CHAR;

		const float charW = 1.5f / width;
		const float charH = 1.5f / height;

		// Starting position for the first letter
		x = (x / width * 2.0f) - 1.0f;
		y = (y / height * 2.0f) - 1.0f;

		// Adjust starting positions for alignments
		if ((align == TextAlign::alignRight) || (align == TextAlign::alignCenter)) {
			// Calculate width for the given string
			float textWidth = 0;
			for (auto letter : text) {
				stb_fontchar* charData = &stbFontData[(uint32_t)letter - firstChar];
				textWidth += charData->advance * charW;
			}
			if (align == TextAlign::alignRight) {
				x -= textWidth;
			}
			if (align == TextAlign::alignCenter) {
				x -= textWidth / 2.0f;
			}
		}

		// Add an UV mapped triangle for each letter to the vertex buffer
		// For simplicity, we won't use a separate index buffer
		for (auto letter : text) {
			stb_fontchar* charData = &stbFontData[(uint32_t)letter - firstChar];
			// First triangle : Top-Left -> Bottom-Left -> Top-Right
			(*target)->x = (x + (float)charData->x0 * charW);
			(*target)->y = (y + (float)charData->y0 * charH);
			(*target)->z = charData->s0;
			(*target)->w = charData->t0;
			(*target)++;
			(*target)->x = (x + (float)charData->x0 * charW);
			(*target)->y = (y + (float)charData->y1 * charH);
			(*target)->z = charData->s0;
			(*target)->w = charData->t1;
			(*target)++;
			(*target)->x = (x + (float)charData->x1 * charW);
			(*target)->y = (y + (float)charData->y0 * charH);
			(*target)->z = charData->s1;
			(*target)->w = charData->t0;
			(*target)++;
			// Second triangle: Bottom-Left -> Bottom-Right -> Top-Right
			(*target)->x = (x + (float)charData->x0 * charW);
			(*target)->y = (y + (float)charData->y1 * charH);
			(*target)->z = charData->s0;
			(*target)->w = charData->t1;
			(*target)++;
			(*target)->x = (x + (float)charData->x1 * charW);
			(*target)->y = (y + (float)charData->y1 * charH);
			(*target)->z = charData->s1;
			(*target)->w = charData->t1;
			(*target)++;
			(*target)->x = (x + (float)charData->x1 * charW);
			(*target)->y = (y + (float)charData->y0 * charH);
			(*target)->z = charData->s1;
			(*target)->w = charData->t0;
			(*target)++;
			// Advance x-offset by the letter's width
			x += charData->advance * charW;
			// Increase vertex count
			overlayVertexCount += 6;
		}
	}

	// Update the buffer for the given frame's text overlay
	void updateTextOverlay(uint32_t frameIndex)
	{
		overlayVertexCount = 0;

		// Map the selected frame's text buffer, so we can write character data into it
		glm::vec4* mappedBuffer = (glm::vec4*)frameObjects[frameIndex].textBuffer.mapped;

		// Display basic information
		addTextToBuffer(&mappedBuffer, title, 5.0f, 5.0f, TextAlign::alignLeft);
		std::stringstream ss;
		ss << std::fixed << std::setprecision(2) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
		addTextToBuffer(&mappedBuffer, ss.str(), 5.0f, 25.0f, TextAlign::alignLeft);
		addTextToBuffer(&mappedBuffer, deviceProperties.deviceName, 5.0f, 45.0f, TextAlign::alignLeft);

		// Display current model view matrix
		addTextToBuffer(&mappedBuffer, "model view matrix", (float)width, 5.0f, TextAlign::alignRight);
		for (uint32_t i = 0; i < 4; i++) {
			ss.str("");
			ss << std::fixed << std::setprecision(2) << std::showpos;
			ss << uniformData.modelView[0][i] << " " << uniformData.modelView[1][i] << " " << uniformData.modelView[2][i] << " " << uniformData.modelView[3][i];
			addTextToBuffer(&mappedBuffer, ss.str(), (float)width, 25.0f + (float)i * 20.0f, TextAlign::alignRight);
		}

		// Display a label at the model's projected screen position
		glm::vec3 projected = glm::project(glm::vec3(0.0f), uniformData.modelView, uniformData.projection, glm::vec4(0, 0, (float)width, (float)height));
		addTextToBuffer(&mappedBuffer, "A cube", projected.x, projected.y, TextAlign::alignCenter);

		// Display controls
#if defined(__ANDROID__)
#else
		addTextToBuffer(&mappedBuffer, "Mouse controls:", 5.0f, 105.0f, TextAlign::alignLeft);
		addTextToBuffer(&mappedBuffer, "Left button: Rotate", 5.0f, 125.0f, TextAlign::alignLeft);
		addTextToBuffer(&mappedBuffer, "Right button: Zoom", 5.0f, 145.0f, TextAlign::alignLeft);
		addTextToBuffer(&mappedBuffer, "Middle button: Move", 5.0f, 165.0f, TextAlign::alignLeft);
		addTextToBuffer(&mappedBuffer, "Press \"space\" to toggle text overlay", 5.0f, 205.0f, TextAlign::alignLeft);
#endif
		// As we don't require a host coherent memory type, flushes are required to make writes visible to the GPU
		frameObjects[frameIndex].textBuffer.flush();
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		model.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1 + getFrameCount());
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layouts
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
		// One layout for the per-frame uniform buffers for rendering the model 
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.uniformbuffers));
		// One layout for the text rendering using the font texture
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.text));

		// Sets
		// Per-frame uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.uniformbuffers, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// Global set for font texture
		VkDescriptorImageInfo fontImageDescriptor = vks::initializers::descriptorImageInfo(fontAtlas.sampler, fontAtlas.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.text, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &fontAtlasDescriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(fontAtlasDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontImageDescriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	void createPipelines()
	{
		// Layouts
		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		// Pipeline layout for rendering the solid model in the background
		pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.uniformbuffers, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayouts.model));
		// Pipeline layout for drawing the text
		pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.text, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayouts.text));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Pipeline for rendering the solid model in the background
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo();
		pipelineCI.renderPass = renderPass;
		pipelineCI.layout = pipelineLayouts.model;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV});
		shaderStages[0] = loadShader(getShadersPath() + "textoverlay/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "textoverlay/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.model));

		// Pipeline for drawing the text
		pipelineCI.layout = pipelineLayouts.text;
		// Enable blending, using alpha from red channel of the font texture (see text.frag)
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		// Vertex input bindings for the text shader
		VkVertexInputBindingDescription vertexInputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX);
		std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, 0),					// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2)),	// Location 1: UV
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
		pipelineCI.pVertexInputState = &vertexInputState;
		shaderStages[0] = loadShader(getShadersPath() + "textoverlay/text.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "textoverlay/text.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.text));
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
		createOverlayResources();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Update the text buffer for the next frame
		updateTextOverlay(getCurrentFrameIndex());

		// Update uniform-buffers for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, clearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// Draw the model in the background
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.model);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.model, 0, 1, &currentFrame.descriptorSet, 0, nullptr);
		model.draw(commandBuffer);

		// Draw the text overlay on top of the scene
		if (showOverlay) {
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.text);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.text, 0, 1, &fontAtlasDescriptorSet, 0, nullptr);
			VkDeviceSize offsets = 0;
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &currentFrame.textBuffer.buffer, &offsets);
			vkCmdDraw(commandBuffer, overlayVertexCount, 1, 0, 0);
		}

		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}

#if !defined(__ANDROID__)
	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_KPADD:
		case KEY_SPACE:
			showOverlay = !showOverlay;
		}
	}
#endif
};

VULKAN_EXAMPLE_MAIN()