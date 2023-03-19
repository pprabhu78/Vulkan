/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "CommandLinerParser.h"

#include "keycodes.hpp"
#include "VulkanUIOverlay.h"
#include "SwapChain.h"
#include "Benchmark.h"
#include "StorageImage.h"

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
   class ApiInstance;
   class PhysicalDevice;
   class Device;

   class PlatformApplication
   {
   public:
      PlatformApplication(bool enableValidation = false);
      virtual ~PlatformApplication();
   public:
      /** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
      bool initVulkan();

      virtual GLFWwindow* setupWindow();

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
      /** @brief (Virtual) Setup a default renderpass */
      virtual void setupRenderPass();
      /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
      virtual void enableFeatures();

      //! This is the final image that will be rendered by OpenGL into its whichever framebuffer
      //! it wants to render into (it can be the default or offscreen framebuffer)
      virtual void setupColor(void);
      virtual void destroyColor(void);

      //! Set up depth-stencil
      virtual void setupDepthStencil();
      virtual void destroyDepthStencil(void);

      //! Set up multi-sample
      virtual void setupMultiSampleColor();
      virtual void destroyMultiSampleColor();

      //! Set up framebuffer
      virtual void setupFrameBuffer();
      virtual void destroyFrameBuffers(void);

      /** @brief Prepares all Vulkan resources and functions required to run the sample */
      virtual void prepare();

      /** @brief Loads a SPIR-V shader file for the given shader stage */
      virtual genesis::Shader* loadShader(std::string fileName, genesis::ShaderType stage);

      /** @brief Entry point for the main render loop */
      virtual void renderLoop();

      /** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
      virtual void drawUI(const VkCommandBuffer commandBuffer);

      /** Prepare the next frame for workload submission by acquiring the next swap chain image */
      virtual void prepareFrame();

      /** @brief Presents the current image to the swap chain */
      virtual void submitFrame();

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
      virtual void onDrop(const std::vector<std::string>& filesDropped);

      virtual VkFormat colorFormat(void) const;

   protected:
      // Returns the path to the root of the glsl or hlsl shader directory.
      virtual std::string getShadersPath() const;

      virtual std::string getAssetsPath(void) const;

      virtual std::string getWindowTitle();

      virtual void setupGlfwCallbacks(GLFWwindow* window);
   private:
      virtual void windowResize();
      virtual void handleMouseMove(int32_t x, int32_t y);
      virtual void nextFrame();
      virtual void postFrame(void);
      virtual void updateOverlay();
      virtual void createPipelineCache();
      virtual void createCommandPool();
      virtual void createSynchronizationPrimitives();
      virtual void initSwapchain();
      virtual void setupSwapChain();
      virtual void createCommandBuffers();
      virtual void destroyCommandBuffers();
   public:
      bool _prepared = false;
      uint32_t _width = 1280 * 2;
      uint32_t _height = 720 * 2;

      genesis::UIOverlay _uiOverlay;
      CommandLineParser _commandLineParser;

      /** @brief Last frame time measured using a high performance timer (if available) */
      float _frameTimer = 1.0f;

      Benchmark _benchmark;

      /** @brief Example settings that can be changed e.g. by command line arguments */
      struct Settings
      {
         /** @brief Activates validation layers (and message output) when set to true */
         bool validation = false;
         /** @brief Set to true if fullscreen mode has been requested via command line */
         bool fullscreen = false;
         /** @brief Set to true if v-sync will be forced for the swapchain */
         bool vsync = false;
         /** @brief Enable UI overlay */
         bool overlay = true;
      } _settings;

      VkClearColorValue _defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

      static std::vector<const char*> _args;

      // Defines a frame rate independent timer value clamped from -1.0...1.0
      // For use in animations, rotations, etc.
      float _timer = 0.0f;

      // Multiplier for speeding up (or slowing down) the global timer
      float _timerSpeed = 0.25f;

      bool _paused = false;

      Camera _camera;

      glm::vec2 _mousePos;

      std::string _title = "Vulkan Example";

      std::string _name = "vulkanExample";

      uint32_t apiVersion = VK_API_VERSION_1_0;

      StorageImage* _depthStencilImage = nullptr;
      StorageImage* _multiSampledColorImage = nullptr;

      struct
      {
         glm::vec2 axisLeft = glm::vec2(0.0f);
         glm::vec2 axisRight = glm::vec2(0.0f);
      } _gamePadState;

      struct
      {
         bool left = false;
         bool right = false;
         bool middle = false;
      } _mouseButtons;

      GLFWwindow* _window = nullptr;

   protected:
      ApiInstance* _instance = nullptr;

      Device* _device = nullptr;

      // Frame counter to display fps
      uint32_t _frameCounter = 0;
      uint32_t _lastFPS = 0;

      std::chrono::time_point<std::chrono::high_resolution_clock> _lastTimestamp;

      std::vector<std::string> _supportedInstanceExtensions;

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
      VkCommandPool _commandPool;

      /** @brief Pipeline stages used to wait at for graphics queue submissions */
      VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      // Contains command buffers and semaphores to be presented to the queue
      VkSubmitInfo _submitInfo;

      // Command buffers used for rendering
      std::vector<VkCommandBuffer> _drawCommandBuffers;

      // Global render pass for frame buffer writes
      RenderPass* _renderPass = 0;

      // List of available frame buffers (same as number of swap chain images)
      std::vector<VkFramebuffer> _frameBuffers;

      // Active frame buffer index
      uint32_t _currentFrameBufferIndex = 0;

      // Descriptor set pool
      VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

      // List of shader modules created (stored for cleanup)
      std::vector<Shader*> _shaders;

      // Pipeline cache object
      VkPipelineCache _pipelineCache;

      // Wraps the swap chain to present images (framebuffers) to the windowing system
      SwapChain* _swapChain = nullptr;

      //! Flow of swap chain rendering:
      //! vkAcquireNextImageKHR(..., semaphoreToSignal, ...)
      //! vkQueueSubmit(..., semaphoreToSignal, semaphoreToWaitOn, ...)
      //! vkQueuePresentKHR(..., semaphoreToWaitOn, ...)
      //! -> 
      //! acquire signals presentComplete
      //! submit waits on presentComplete and signals renderComplete
      //! present waits on renderComplete
      struct
      {
         //! This is passed to vkAcquireNextImageKHR.
         //! It gets signaled when the image index acquired can actually be rendered to.
         //! This is because the presentation engine may still be reading from that image (because of earlier call to vkQueuePresentKHR)
         //! This is the semaphore that vkQueueSubmit will _wait_ on
         VkSemaphore presentComplete;

         //! This is passed as the one of the pSignalSemaphores to VkSubmitInfo for vkQueueSubmit.
         //! It gets signaled after the command buffers to vkQueueSubmit have been actually executed.
         //! This is the semaphore that vkQueuePresentKHR will _wait_ on
         VkSemaphore renderComplete;
      } _semaphores;

      std::vector<VkFence> _waitFences;

      //! If dynamic rendering is true, there is no need to create render pass
      //! or frame buffers
      bool _dynamicRendering = false;

      //! The anti-aliasing level
      int _sampleCount = 1;

      bool viewUpdated = false;
      uint32_t destWidth;
      uint32_t destHeight;
      bool resizing = false;

      std::string shaderDir = "glsl";

      //! If swap chain rendering is false, the image is rendered to the color image below
      bool _useSwapChainRendering = true;
      VkFormat _colorFormatGlRendering = VK_FORMAT_R8G8B8A8_UNORM;
      StorageImage* _colorImage = nullptr;
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