/*
 * Vulkan Example - CPU based particle system
 *
 * Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * This sample shows how to implement a CPU-based particle system using point sprites
 * It implements an animated particle-based fire with random particles that transition between flame and smoke over their lifetime, before being respawn again
 * The particles are updated on the CPU for each frame and are then passed to the vertex shader using a host visible buffer
 * This host visible buffer is duplicated per frame, so we can start to update buffer n+1 while the command buffer using buffer n is still being executed
 * The sample also shows how to ensure that updates to the host vertex buffer are visible to the device
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	// Parameters for the particle system setup
	const float flameRadius = 8.0f;
	const uint32_t particleCount = 512;
	glm::vec3 emitterPos = glm::vec3(0.0f, -flameRadius + 2.0f, 0.0f);
	glm::vec3 minVel = glm::vec3(-3.0f, 0.5f, -3.0f);
	glm::vec3 maxVel = glm::vec3(3.0f, 7.0f, 3.0f);

	// Stores the state of a single particle
	enum class ParticleType { Flame = 0, Smoke = 1 };
	struct Particle {
		glm::vec4 position;
		glm::vec4 color;
		// Alpha is used for transparency and life time of a particle
		float alpha;
		float size;
		float rotation;
		ParticleType type;
		glm::vec4 velocity;
		float rotationSpeed;
	};
	// Stores the current state of all particles
	std::vector<Particle> particles;
	// Wraps access to the host visible Vulkan buffer storing the particle data that is passed to the vertex shader
	// It's updated for each frame from the above particles vector
	struct ParticleBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
		void* mappedMemory;
		size_t size;
	};

	struct Textures {
		vks::Texture2D smokeParticle;
		vks::Texture2D fireParticle;
		VkSampler sampler;
	} textures;

	vkglTF::Model environment;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::mat4 normal;
		glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		glm::vec2 viewportDim;
		float pointSize = 10.0f;
	} uniformData;

	struct FrameObjects : public VulkanFrameObjects {
		vks::Buffer uniformBuffer;
		VkDescriptorSet descriptorSet;
		// We use one vertex buffer for each frame, so we can update buffer n+1 while buffer n is still in use
		ParticleBuffer particleBuffer;
	};
	std::vector<FrameObjects> frameObjects;

	struct Pipelines {
		VkPipeline particles;
		VkPipeline environment;
	} pipelines;
	VkPipelineLayout pipelineLayout;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout uniformbuffers;
		VkDescriptorSetLayout images;
	} descriptorSetLayouts;
	// The descriptor set for the images is static, and not required to be per-frame
	VkDescriptorSet imagesDescriptorSet;

	std::default_random_engine rndEngine;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "CPU based particle system";
		camera.setType(Camera::CameraType::lookat);
		camera.setPosition(glm::vec3(0.0f, 15.0f, -50.0f));
		camera.setRotation(glm::vec3(-15.0f, 45.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
		settings.overlay = true;
		timerSpeed *= 8.0f;
		rndEngine.seed(benchmark.active ? 0 : (unsigned)time(nullptr));
	}

	~VulkanExample()
	{
		if (device) {
			textures.smokeParticle.destroy();
			textures.fireParticle.destroy();
			vkDestroyPipeline(device, pipelines.particles, nullptr);
			vkDestroyPipeline(device, pipelines.environment, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.uniformbuffers, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.images, nullptr);
			vkDestroySampler(device, textures.sampler, nullptr);
			for (FrameObjects& frame : frameObjects) {
				vkDestroyBuffer(device, frame.particleBuffer.buffer, nullptr);
				vkFreeMemory(device, frame.particleBuffer.memory, nullptr);
				frame.uniformBuffer.destroy();
				destroyBaseFrameObjects(frame);
			}
		}
	}

	virtual void getEnabledFeatures()
	{
		// Enable anisotropic filtering if supported
		enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	}

	// Returns a random value between 0.0 and the given range
	float rnd(float range)
	{
		std::uniform_real_distribution<float> rndDist(0.0f, range);
		return rndDist(rndEngine);
	}

	// Initialize all particles with random properties
	void initParticles()
	{
		particles.resize(particleCount);
		for (Particle& particle : particles) {
			resetParticle(particle, emitterPos);
			particle.alpha = 1.0f - (abs(particle.position.y) / (flameRadius * 2.0f));
		}
	}

	// Reset a particle to a default state with randomized properties 
	void resetParticle(Particle& particle, glm::vec3 emitterPos)
	{
		particle.velocity = glm::vec4(0.0f, minVel.y + rnd(maxVel.y - minVel.y), 0.0f, 0.0f);
		particle.alpha = rnd(0.75f);
		particle.size = 1.0f + rnd(0.5f);
		particle.color = glm::vec4(1.0f);
		particle.rotation = rnd(2.0f * float(M_PI));
		particle.rotationSpeed = rnd(2.0f) - rnd(2.0f);
		particle.type = ParticleType::Flame;
		// Get a random point on a sphere at the center of the fire emitter
		float theta = rnd(2.0f * float(M_PI));
		float phi = rnd(float(M_PI)) - float(M_PI) / 2.0f;
		float r = rnd(flameRadius);
		particle.position.x = r * cos(theta) * cos(phi);
		particle.position.y = r * sin(phi);
		particle.position.z = r * sin(theta) * cos(phi);
		particle.position += glm::vec4(emitterPos, 0.0f);
	}

	// Advances the particle system based on the last frame time
	void updateParticles()
	{
		float particleTimer = frameTimer * 0.45f;
		for (Particle& particle : particles) {
			switch (particle.type)
			{
			case ParticleType::Flame:
				particle.position.y -= particle.velocity.y * particleTimer * 3.5f;
				particle.alpha += particleTimer * 2.5f;
				particle.size -= particleTimer * 0.5f;
				break;
			case ParticleType::Smoke:
				particle.position -= particle.velocity * frameTimer * 1.0f;
				particle.alpha += particleTimer * 1.25f;
				particle.size += particleTimer * 0.125f;
				particle.color -= particleTimer * 0.05f;
				break;
			}
			particle.rotation += particleTimer * particle.rotationSpeed;
			// Once a particle has reached the end of it's current cycle we transition it e.g. from flame to smoke, or reset it
			if (particle.alpha > 2.0f) {
				switch (particle.type) 
				{
				case ParticleType::Flame:
					// Flame particles have a small chance of turning into smoke
					if (rnd(1.0f) < 0.05f) {
						particle.alpha = 0.0f;
						particle.color = glm::vec4(0.25f + rnd(0.25f));
						particle.position.x *= 0.5f;
						particle.position.z *= 0.5f;
						particle.velocity = glm::vec4(rnd(1.0f) - rnd(1.0f), (minVel.y * 2) + rnd(maxVel.y - minVel.y), rnd(1.0f) - rnd(1.0f), 0.0f);
						particle.size = 1.0f + rnd(0.5f);
						particle.rotationSpeed = rnd(1.0f) - rnd(1.0f);
						particle.type = ParticleType::Smoke;
					} else {
						// Or they'll get reset
						resetParticle(particle, emitterPos);
					}
					break;
				case ParticleType::Smoke:
					// Smoke particles will always respawn at the center of the fire at the end of their current cycle
					resetParticle(particle, emitterPos);
					break;
				}
			}
		}
	}

	void loadAssets()
	{
		// Particle textures
		textures.smokeParticle.loadFromFile(getAssetPath() + "textures/particle_smoke.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.fireParticle.loadFromFile(getAssetPath() + "textures/particle_fire.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		// Create a custom sampler for drawing the alpha-blended particles 
		VkSamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		// Both particle textures have the same number of mip maps
		samplerCreateInfo.maxLod = float(textures.smokeParticle.mipLevels);
		// Enable anisotropic filtering if available
		if (vulkanDevice->features.samplerAnisotropy) {
			samplerCreateInfo.maxAnisotropy = 8.0f;
			samplerCreateInfo.anisotropyEnable = VK_TRUE;
		}
		// Use a different border color (than the normal texture loader) for additive blending
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &textures.sampler));

		// Load the background from a glTF file
		// Tell the glTF loader to create and bind descriptors for color and normal maps, so we can apply normal mapping in our shader
		vkglTF::descriptorBindingFlags = vkglTF::DescriptorBindingFlags::ImageBaseColor | vkglTF::DescriptorBindingFlags::ImageNormalMap;
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		environment.loadFromFile(getAssetPath() + "models/fireplace.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void createDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, getFrameCount()),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, getFrameCount() + 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Layouts
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
		// One layout for the per-frame uniform buffers
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.uniformbuffers));
		// One layout for the images used for the particles
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.images));

		// Sets
		// Per-frame for dynamic uniform buffers
		for (FrameObjects& frame : frameObjects) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.uniformbuffers, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &frame.uniformBuffer.descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// Global set for the different particle textures 
		VkDescriptorImageInfo imageDescriptorSmokeParticle = vks::initializers::descriptorImageInfo(textures.sampler, textures.smokeParticle.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorImageInfo imageDescriptorFireParticle = vks::initializers::descriptorImageInfo(textures.sampler, textures.fireParticle.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.images, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &imagesDescriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(imagesDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptorSmokeParticle),
			vks::initializers::writeDescriptorSet(imagesDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptorFireParticle),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void createPipelines()
	{
		// Layout 
		const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.uniformbuffers, descriptorSetLayouts.images };
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Particle properties are passed to the vertex shader via vertex bindings and attributes (see particle.vert)
		VkVertexInputBindingDescription particleVertexInputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);
		std::vector<VkVertexInputAttributeDescription> particleVertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(Particle, position)),	// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(Particle, color)),		// Location 1: Color
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32_SFLOAT, offsetof(Particle, alpha)),				// Location 2: Alpha
			vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32_SFLOAT, offsetof(Particle, size)),				// Location 3: Size
			vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(Particle, rotation)),			// Location 4: Rotation
			vks::initializers::vertexInputAttributeDescription(0, 5, VK_FORMAT_R32_SINT, offsetof(Particle, type)),					// Location 5: Particle type
		};
		VkPipelineVertexInputStateCreateInfo particleVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		particleVertexInputState.vertexBindingDescriptionCount = 1;
		particleVertexInputState.pVertexBindingDescriptions = &particleVertexInputBinding;
		particleVertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(particleVertexInputAttributes.size());
		particleVertexInputState.pVertexAttributeDescriptions = particleVertexInputAttributes.data();

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo();
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

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

		// Pipeline for rendering the alpha blended particles
		// The particles are rendered as point sprites
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		pipelineCI.pVertexInputState = &particleVertexInputState;
		// Disable depth writes, so we don't need to do manual sort the particles
		depthStencilState.depthWriteEnable = VK_FALSE;
		// The particle textures are using premultiplied alpha, so we enable blending and set the blend factors accordingly
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		shaderStages[0] = loadShader(getShadersPath() + "cpuparticles/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "cpuparticles/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.particles));

		// Pipeline for rendering the normal mapped environment model
		// The scene is rendered as triangles
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// The vertex input state for this pipeline is taken from the glTF model loader
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Tangent });
		// Disable blending and enable depth writes again
		blendAttachmentState.blendEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		shaderStages[0] = loadShader(getShadersPath() + "cpuparticles/normalmap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "cpuparticles/normalmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.environment));
	}

	// Create the host visible buffers to store the particle properties
	void createParticleBuffers()
	{
		// Create a host visible buffer to store particle properties passed to the vertex shader for each frame
		for (FrameObjects& frame : frameObjects) {
			frame.particleBuffer.size = particles.size() * sizeof(Particle);
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				// We'll manually flush the mapped memory ranges, so we won't be using host coherent memory for this sample
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				frame.particleBuffer.size,
				&frame.particleBuffer.buffer,
				&frame.particleBuffer.memory,
				particles.data()));
			// Map the memory persitent
			VK_CHECK_RESULT(vkMapMemory(device, frame.particleBuffer.memory, 0, frame.particleBuffer.size, 0, &frame.particleBuffer.mappedMemory));
		}
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		// Prepare per-frame ressources
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(UniformData)));
		}
		loadAssets();
		initParticles();
		createParticleBuffers();
		createDescriptors();
		createPipelines();
		prepared = true;
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		// Advance the particle system
		if (!paused) {
			updateParticles();
			// Update the vertex buffer containing the particles for the next frame
			size_t size = particles.size() * sizeof(Particle);
			memcpy(currentFrame.particleBuffer.mappedMemory, particles.data(), size);
			// We manually flush the mapped memory ranges to make the host writes visible to the device
			VkMappedMemoryRange mappedRange{};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = currentFrame.particleBuffer.memory;
			mappedRange.offset = 0;
			mappedRange.size = size;
			vkFlushMappedMemoryRanges(device, 1, &mappedRange);
		}

		// Update uniform-buffers for the next frame
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelview = camera.matrices.view * glm::mat4(1.0f);
		uniformData.normal = glm::inverseTranspose(uniformData.modelview);
		uniformData.viewportDim = glm::vec2((float)width, (float)height);
		uniformData.lightPos = glm::vec4(sin(timer * 2.0f * float(M_PI)) * 1.5f, 0.0f, cos(timer * 2.0f * float(M_PI)) * 1.5f, 0.0f);
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));

		// Build the command buffer
		const VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
		const VkCommandBufferBeginInfo commandBufferBeginInfo = getCommandBufferBeginInfo();
		const VkRect2D renderArea = getRenderArea();
		const VkViewport viewport = getViewport();
		VkClearValue clearValues[2] = { { 0.4f, 0.4f, 0.4f, 1.0f }, { 1.0f, 0 } };
		const VkRenderPassBeginInfo renderPassBeginInfo = getRenderPassBeginInfo(renderPass, clearValues);
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		// Bind the uniform buffers to set 0
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &currentFrame.descriptorSet, 0, nullptr);

		// Draw the enviornment
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.environment);
		environment.draw(commandBuffer, vkglTF::RenderFlags::BindImages, pipelineLayout);

		// Draw the particle system
		VkDeviceSize offsets[1] = { 0 };
		// Bind the particle images to set 1
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &imagesDescriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.particles);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &currentFrame.particleBuffer.buffer, offsets);
		vkCmdDraw(commandBuffer, particleCount, 1, 0, 0);

		drawUI(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VulkanExampleBase::submitFrame(currentFrame);
	}
};

VULKAN_EXAMPLE_MAIN()