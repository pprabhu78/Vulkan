/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "CommandLinerParser.h"

#include "keycodes.hpp"
#include "VulkanUIOverlay.h"
#include "VulkanSwapChain.h"
#include "Benchmark.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>
#include <string>
#include <numeric>
#include <array>

#include "vulkan/vulkan.h"

#include "Camera.h"
#include "Shader.h"


struct GLFWwindow;

namespace genesis
{
	class RenderPass;
	class Instance;
	class PhysicalDevice;
	class Device;

	class VulkanExampleBase
	{
	private:
		std::string getWindowTitle();
		bool viewUpdated = false;
		uint32_t destWidth;
		uint32_t destHeight;
		bool resizing = false;
		void windowResize();
		void handleMouseMove(int32_t x, int32_t y);
		void nextFrame();
		void updateOverlay();
		void createPipelineCache();
		void createCommandPool();
		void createSynchronizationPrimitives();
		void initSwapchain();
		void setupSwapChain();
		void createCommandBuffers();
		void destroyCommandBuffers();
		std::string shaderDir = "glsl";
	protected:
		Instance* _instance;

      Device* _device;

		// Returns the path to the root of the glsl or hlsl shader directory.
		std::string getShadersPath() const;

		std::string getAssetsPath(void) const;

		// Frame counter to display fps
		uint32_t frameCounter = 0;
		uint32_t lastFPS = 0;
		std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;
		std::vector<std::string> supportedInstanceExtensions;
		
		// Physical device (GPU) that Vulkan will use
		PhysicalDevice* _physicalDevice;


		//! instance extensions to enable
		std::vector<std::string> _enabledInstanceExtensions;

		/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
		std::vector<const char*> _enabledPhysicalDeviceExtensions;

		/** @brief Optional pNext structure for passing extension structures to device creation */
		void* deviceCreatepNextChain = nullptr;
		
		// Depth buffer format (selected during Vulkan initialization)
		VkFormat _depthFormat;
		// Command buffer pool
		VkCommandPool cmdPool;
		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submitInfo;
		// Command buffers used for rendering
		std::vector<VkCommandBuffer> drawCmdBuffers;
		// Global render pass for frame buffer writes
		RenderPass* _renderPass;
		// List of available frame buffers (same as number of swap chain images)
		std::vector<VkFramebuffer>frameBuffers;
		// Active frame buffer index
		uint32_t currentBuffer = 0;
		// Descriptor set pool
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		// List of shader modules created (stored for cleanup)
		std::vector<Shader*> _shaders;
		// Pipeline cache object
		VkPipelineCache pipelineCache;
		// Wraps the swap chain to present images (framebuffers) to the windowing system
		VulkanSwapChain swapChain;
		// Synchronization semaphores
		struct {
			// Swap chain image presentation
			VkSemaphore presentComplete;
			// Command buffer submission and execution
			VkSemaphore renderComplete;
		} semaphores;
		std::vector<VkFence> waitFences;
	public:
		bool prepared = false;
		bool resized = false;
		uint32_t width = 1280 * 2;
		uint32_t height = 720 * 2;

		genesis::UIOverlay UIOverlay;
		CommandLineParser commandLineParser;

		/** @brief Last frame time measured using a high performance timer (if available) */
		float frameTimer = 1.0f;

		Benchmark benchmark;

		/** @brief Example settings that can be changed e.g. by command line arguments */
		struct Settings {
			/** @brief Activates validation layers (and message output) when set to true */
			bool validation = false;
			/** @brief Set to true if fullscreen mode has been requested via command line */
			bool fullscreen = false;
			/** @brief Set to true if v-sync will be forced for the swapchain */
			bool vsync = false;
			/** @brief Enable UI overlay */
			bool overlay = true;
		} settings;

		VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

		static std::vector<const char*> args;

		// Defines a frame rate independent timer value clamped from -1.0...1.0
		// For use in animations, rotations, etc.
		float timer = 0.0f;
		// Multiplier for speeding up (or slowing down) the global timer
		float timerSpeed = 0.25f;
		bool paused = false;

		Camera camera;
		glm::vec2 mousePos;

		std::string title = "Vulkan Example";
		std::string name = "vulkanExample";
		uint32_t apiVersion = VK_API_VERSION_1_0;

		struct {
			VkImage image;
			VkDeviceMemory mem;
			VkImageView view;
		} depthStencil;

		struct {
			glm::vec2 axisLeft = glm::vec2(0.0f);
			glm::vec2 axisRight = glm::vec2(0.0f);
		} gamePadState;

		struct {
			bool left = false;
			bool right = false;
			bool middle = false;
		} mouseButtons;

		GLFWwindow* window;

		VulkanExampleBase(bool enableValidation = false);
		virtual ~VulkanExampleBase();
		/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
		bool initVulkan();

		GLFWwindow* setupWindow();
		void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		/** @brief (Virtual) Creates the application wide Vulkan instance */
		virtual VkResult createInstance(bool enableValidation);
		/** @brief (Pure virtual) Render function to be implemented by the sample application */
		virtual void render() = 0;
		/** @brief (Virtual) Called when the camera view has changed */
		virtual void viewChanged();
		/** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
		virtual void keyPressed(uint32_t);
		/** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
		virtual void mouseMoved(double x, double y, bool& handled);
		/** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
		virtual void windowResized();
		/** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
		virtual void buildCommandBuffers();
		/** @brief (Virtual) Setup default depth and stencil views */
		virtual void setupDepthStencil();
		/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
		virtual void setupFrameBuffer();
		/** @brief (Virtual) Setup a default renderpass */
		virtual void setupRenderPass();
		/** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
		virtual void getEnabledFeatures();

		/** @brief Prepares all Vulkan resources and functions required to run the sample */
		virtual void prepare();

		/** @brief Loads a SPIR-V shader file for the given shader stage */
		genesis::Shader* loadShader(std::string fileName, genesis::ShaderType stage);

		/** @brief Entry point for the main render loop */
		void renderLoop();

		/** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
		void drawUI(const VkCommandBuffer commandBuffer);

		/** Prepare the next frame for workload submission by acquiring the next swap chain image */
		void prepareFrame();
		/** @brief Presents the current image to the swap chain */
		void submitFrame();
		/** @brief (Virtual) Default image acquire + submission and command buffer submission function */
		virtual void renderFrame();

		/** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
		virtual void OnUpdateUIOverlay(genesis::UIOverlay* overlay);

		// called by glfw callback
		virtual void onKeyboard(int key, int scancode, int action, int mods);
		virtual void onMouseButton(int button, int action, int mods);
		virtual void onMouseMotion(int x, int y);
		virtual void onMouseWheel(int delta);
		virtual void onFramebufferSize(int w, int h);
	};

	// OS specific macros for the example main entry points

// Windows entry point
#define VULKAN_EXAMPLE_MAIN()																		\
VulkanExample *vulkanExample;																		\
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						\
{																									\
	if (vulkanExample != NULL)																		\
	{																								\
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);									\
	}																								\
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));												\
}																									\
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)									\
{																									\
	for (int32_t i = 0; i < __argc; i++) { VulkanExample::args.push_back(__argv[i]); };  			\
	vulkanExample = new VulkanExample();															\
	vulkanExample->initVulkan();																	\
	vulkanExample->setupWindow(hInstance, WndProc);													\
	vulkanExample->prepare();																		\
	vulkanExample->renderLoop();																	\
	delete(vulkanExample);																			\
	return 0;																						\
}

}