/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "PlatformApplication.h"
#include "RenderPass.h"
#include "Instance.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "VulkanInitializers.h"
#include "VkExtensions.h"


#ifdef VK_USE_PLATFORM_GLFW
#if _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "../external/glfw/include/GLFW/glfw3.h"
#include "../external/glfw/include/GLFW/glfw3native.h"
#endif

namespace genesis
{
   std::vector<const char*> PlatformApplication::_args;

   VkResult PlatformApplication::createInstance(bool enableValidation)
   {
      this->_settings.validation = enableValidation;

      // Validation can also be forced via a define
#if defined(_VALIDATION)
      this->_settings.validation = true;
#endif

      _instance = new ApiInstance(_name, _enabledInstanceExtensions, apiVersion, this->_settings.validation);
      return _instance->creationStatus();
   }

   void PlatformApplication::renderFrame()
   {
      PlatformApplication::prepareFrame();
      _submitInfo.commandBufferCount = 1;
      _submitInfo.pCommandBuffers = &_drawCommandBuffers[_currentFrameBufferIndex];
      VK_CHECK_RESULT(vkQueueSubmit(_device->graphicsQueue(), 1, &_submitInfo, VK_NULL_HANDLE));
      PlatformApplication::submitFrame();
   }

   std::string PlatformApplication::getWindowTitle()
   {
      std::string device(_physicalDevice->physicalDeviceProperties().deviceName);
      std::string windowTitle;
      windowTitle = _title + " - " + device;
      if (!_settings.overlay) {
         windowTitle += " - " + std::to_string(_frameCounter) + " fps";
      }
      return windowTitle;
   }

   void PlatformApplication::createCommandBuffers()
   {
      // Create one command buffer for each swap chain image and reuse for rendering
      int imageCount = (_useSwapChainRendering) ? _swapChain->imageCount() : 3;
      _drawCommandBuffers.resize(imageCount);

      VkCommandBufferAllocateInfo cmdBufAllocateInfo =
         vkInitaliazers::commandBufferAllocateInfo(
            _commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<uint32_t>(_drawCommandBuffers.size()));

      VK_CHECK_RESULT(vkAllocateCommandBuffers(_device->vulkanDevice(), &cmdBufAllocateInfo, _drawCommandBuffers.data()));
   }

   void PlatformApplication::destroyCommandBuffers()
   {
      if (_drawCommandBuffers.empty())
      {
         return;
      }
      vkFreeCommandBuffers(_device->vulkanDevice(), _commandPool, static_cast<uint32_t>(_drawCommandBuffers.size()), _drawCommandBuffers.data());
   }

   std::string PlatformApplication::getAssetsPath() const
   {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
      return "";
#elif defined(VK_EXAMPLE_DATA_DIR)
      return VK_EXAMPLE_DATA_DIR;
#else
      return "./../data/";
#endif
   }

   std::string PlatformApplication::getShadersPath() const
   {
      return getAssetsPath() + "shaders/" + shaderDir + "/";
   }

   void PlatformApplication::createPipelineCache()
   {
      VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
      pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
      VK_CHECK_RESULT(vkCreatePipelineCache(_device->vulkanDevice(), &pipelineCacheCreateInfo, nullptr, &_pipelineCache));
   }

   void PlatformApplication::prepare()
   {
      if (_device->enableDebugMarkers()) {
         genesis::debugmarker::setup(_device->vulkanDevice());
      }

      if (_useSwapChainRendering)
      {
         initSwapchain();
         setupSwapChain();
      }
      createCommandPool();
      createCommandBuffers();
      createSynchronizationPrimitives();
      if (!_useSwapChainRendering)
      {
         setupColor();
      }
      setupMultiSampleColor();
      setupDepthStencil();
      if (_dynamicRendering == false)
      {
         setupRenderPass();
      }
      createPipelineCache();
      if (_dynamicRendering==false)
      {
         setupFrameBuffer();
      }
      _settings.overlay = _settings.overlay && (!_benchmark.active);

      _uiOverlay._device = _device;
      _uiOverlay._shaders.push_back(loadShader(getShadersPath() + "genesis/uioverlay.vert.spv", genesis::ST_VERTEX_SHADER));
      _uiOverlay._shaders.push_back(loadShader(getShadersPath() + "genesis/uioverlay.frag.spv", genesis::ST_FRAGMENT_SHADER));
      _uiOverlay._rasterizationSamples = Image::toSampleCountFlagBits(_sampleCount);
      _uiOverlay.prepareResources();
      _uiOverlay.preparePipeline(_pipelineCache, (_renderPass) ? _renderPass->vulkanRenderPass() : nullptr, colorFormat(), _depthFormat);
   }

   genesis::Shader* PlatformApplication::loadShader(std::string fileName, genesis::ShaderType stage)
   {
      genesis::Shader* shader = new genesis::Shader(_device);
      shader->loadFromFile(fileName, stage);
      if (shader->valid() == false)
      {
         std::cout << "error loading shader" << std::endl;
         delete shader;
         return nullptr;
      }
      _shaders.push_back(shader);
      return shader;
   }

   void PlatformApplication::nextFrame()
   {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated)
      {
         viewUpdated = false;
         viewChanged();
      }

      render();
      _frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      _frameTimer = (float)tDiff / 1000.0f;
      _camera.update(_frameTimer);
      if (_camera.moving())
      {
         viewUpdated = true;
      }
      // Convert to clamped timer value
      if (!_paused)
      {
         _timer += _timerSpeed * _frameTimer;
         if (_timer > 1.0)
         {
            _timer -= 1.0f;
         }
      }
      float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - _lastTimestamp).count());
      if (fpsTimer > 1000.0f)
      {
         _lastFPS = static_cast<uint32_t>((float)_frameCounter * (1000.0f / fpsTimer));
#if defined(_WIN32)
         if (!_settings.overlay) {
            std::string windowTitle = getWindowTitle();
#if VK_USE_PLATFORM_GLFW
            glfwSetWindowTitle(_window, windowTitle.c_str());
#else
            SetWindowText(_window, windowTitle.c_str());
#endif
         }
#endif
         _frameCounter = 0;
         _lastTimestamp = tEnd;
      }
      // TODO: Cap UI overlay update rates
      updateOverlay();
   }

   void PlatformApplication::postFrame(void)
   {
      // no op
   }

   static bool IsMinimized(GLFWwindow* window)
   {
      int w, h;
      glfwGetWindowSize(window, &w, &h);
      bool minimized = (w == 0 && h == 0);
      return minimized;
   }

   void PlatformApplication::renderLoop()
   {
      if (_benchmark.active) {
         _benchmark.run([=] { render(); }, _physicalDevice->physicalDeviceProperties());
         vkDeviceWaitIdle(_device->vulkanDevice());
         if (_benchmark.filename != "") {
            _benchmark.saveResults();
         }
         return;
      }

      destWidth = _width;
      destHeight = _height;
      _lastTimestamp = std::chrono::high_resolution_clock::now();

      bool quitMessageReceived = false;
      while (!glfwWindowShouldClose(_window))
      {
         glfwPollEvents();

         if (_prepared && !IsMinimized(_window)) 
         {
            nextFrame();
         }
         postFrame();
      }

      // Flush device to make sure all resources can be freed
      if (_device->vulkanDevice() != VK_NULL_HANDLE) 
      {
         vkDeviceWaitIdle(_device->vulkanDevice());
      }

      glfwDestroyWindow(_window);
      glfwTerminate();
   }

   void PlatformApplication::updateOverlay()
   {
      if (!_settings.overlay)
         return;

      ImGuiIO& io = ImGui::GetIO();

      io.DisplaySize = ImVec2((float)_width, (float)_height);
      io.DeltaTime = _frameTimer;

      io.MousePos = ImVec2(_mousePos.x, _mousePos.y);
      io.MouseDown[0] = _mouseButtons.left;
      io.MouseDown[1] = _mouseButtons.right;

      ImGui::NewFrame();

      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
      ImGui::SetNextWindowPos(ImVec2(10, 10));
      ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
      ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
      ImGui::TextUnformatted(_title.c_str());
      ImGui::TextUnformatted(_physicalDevice->physicalDeviceProperties().deviceName);
      ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / _lastFPS), _lastFPS);


      ImGui::PushItemWidth(110.0f * _uiOverlay._scale);
      OnUpdateUIOverlay(&_uiOverlay);
      ImGui::PopItemWidth();


      ImGui::End();
      ImGui::PopStyleVar();
      ImGui::Render();

      if (_uiOverlay.update() || _uiOverlay._updated) {
         buildCommandBuffers();
         _uiOverlay._updated = false;
      }

   }

   void PlatformApplication::drawUI(const VkCommandBuffer commandBuffer)
   {
      if (_settings.overlay == false)
      {
         return;
      }

      const VkViewport viewport = vkInitaliazers::viewport((float)_width, (float)_height, 0.0f, 1.0f, false);
      const VkRect2D scissor = vkInitaliazers::rect2D(_width, _height, 0, 0);
      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

      _uiOverlay.draw(commandBuffer);
   }

   void PlatformApplication::prepareFrame()
   {
      if (!_useSwapChainRendering)
      {
         return;
      }
      // Acquire the next image from the swap chain
      // presentComplete is the semaphore to signal 
      VkResult result = _swapChain->acquireNextImage(_currentFrameBufferIndex, _semaphores.presentComplete);
      // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
      if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) 
      {
         windowResize();
      }
      else 
      {
         VK_CHECK_RESULT(result);
      }
   }

   void PlatformApplication::submitFrame()
   {
      if (!_useSwapChainRendering)
      {
         return;
      }
      VkResult result = _swapChain->queuePresent(_device->graphicsQueue(), _currentFrameBufferIndex, _semaphores.renderComplete);
      if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
      {
         if (result == VK_ERROR_OUT_OF_DATE_KHR) 
         {
            // Swap chain is no longer compatible with the surface and needs to be recreated
            windowResize();
            return;
         }
         else 
         {
            VK_CHECK_RESULT(result);
         }
      }
      VK_CHECK_RESULT(vkQueueWaitIdle(_device->graphicsQueue()));
   }

   PlatformApplication::PlatformApplication(bool enableValidation)
      : _device(nullptr)
   {
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
      // Check for a valid asset path
      struct stat info;
      if (stat(getAssetsPath().c_str(), &info) != 0)
      {
#if defined(_WIN32)
         std::string msg = "Could not locate asset path in \"" + getAssetsPath() + "\" !";
         MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
#else
         std::cerr << "Error: Could not find asset path in " << getAssetPath() << "\n";
#endif
         exit(-1);
      }
#endif

      _settings.validation = enableValidation;

      // Command line arguments
      _commandLineParser.parse(_args);
      if (_commandLineParser.isSet("help"))
      {
         _commandLineParser.printHelp();
         std::cin.get();
         exit(0);
      }
      if (_commandLineParser.isSet("validation")) {
         _settings.validation = true;
      }
      if (_commandLineParser.isSet("vsync")) {
         _settings.vsync = true;
      }
      if (_commandLineParser.isSet("height")) {
         _height = _commandLineParser.getValueAsInt("height", _width);
      }
      if (_commandLineParser.isSet("width")) {
         _width = _commandLineParser.getValueAsInt("width", _width);
      }
      if (_commandLineParser.isSet("fullscreen")) {
         _settings.fullscreen = true;
      }
      if (_commandLineParser.isSet("shaders")) {
         std::string value = _commandLineParser.getValueAsString("shaders", "glsl");
         if ((value != "glsl") && (value != "hlsl")) {
            std::cerr << "Shader type must be one of 'glsl' or 'hlsl'\n";
         }
         else {
            shaderDir = value;
         }
      }
      if (_commandLineParser.isSet("benchmark")) {
         _benchmark.active = true;
      }
      if (_commandLineParser.isSet("benchmarkwarmup")) {
         _benchmark.warmup = _commandLineParser.getValueAsInt("benchmarkwarmup", _benchmark.warmup);
      }
      if (_commandLineParser.isSet("benchmarkruntime")) {
         _benchmark.duration = _commandLineParser.getValueAsInt("benchmarkruntime", _benchmark.duration);
      }
      if (_commandLineParser.isSet("benchmarkresultfile")) {
         _benchmark.filename = _commandLineParser.getValueAsString("benchmarkresultfile", _benchmark.filename);
      }
      if (_commandLineParser.isSet("benchmarkresultframes")) {
         _benchmark.outputFrameTimes = true;
      }
      if (_commandLineParser.isSet("benchmarkframes")) {
         _benchmark.outputFrames = _commandLineParser.getValueAsInt("benchmarkframes", _benchmark.outputFrames);
      }
   }

   PlatformApplication::~PlatformApplication()
   {
      delete _swapChain;

      if (descriptorPool != VK_NULL_HANDLE)
      {
         vkDestroyDescriptorPool(_device->vulkanDevice(), descriptorPool, nullptr);
      }
      destroyCommandBuffers();
      delete _renderPass;
      if (_dynamicRendering==false)
      {
         destroyFrameBuffers();
      }
      for (Shader* shader : _shaders)
      {
         delete shader;
      }

      destroyDepthStencil();
      destroyMultiSampleColor();
      destroyColor();

      if (_device)
      {
         vkDestroyPipelineCache(_device->vulkanDevice(), _pipelineCache, nullptr);

         vkDestroyCommandPool(_device->vulkanDevice(), _commandPool, nullptr);

         vkDestroySemaphore(_device->vulkanDevice(), _semaphores.presentComplete, nullptr);
         vkDestroySemaphore(_device->vulkanDevice(), _semaphores.renderComplete, nullptr);
         for (auto& fence : _waitFences) 
         {
            vkDestroyFence(_device->vulkanDevice(), fence, nullptr);
         }
      }

      _uiOverlay.freeResources();

      delete _device;
      delete _instance;
   }

   bool PlatformApplication::initVulkan()
   {
      VkResult err;

      // Vulkan instance
      err = createInstance(_settings.validation);
      if (err) {
         tools::exitFatal("Could not create Vulkan instance : \n" + tools::errorString(err), err);
         return false;
      }

      bool ok = _instance->enumeratePhysicalDevices();
      if (!ok)
      {
         std::cout << "Could not enumerate physical devices : \n" << VulkanErrorToString::toString(err) << std::endl;
         return false;
      }

      // GPU selection

      // Select physical device to be used for the Vulkan example
      // Defaults to the first device unless specified by command line
      uint32_t selectedDevice = 0;

      size_t gpuCount = _instance->physicalDevices().size();

#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
      // GPU selection via command line argument
      if (_commandLineParser.isSet("gpuselection")) {
         uint32_t index = _commandLineParser.getValueAsInt("gpuselection", 0);
         if (index > gpuCount - 1) {
            std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
         }
         else {
            selectedDevice = index;
         }
      }
      if (_commandLineParser.isSet("gpulist")) {
         std::cout << "Available Vulkan devices" << "\n";
         for (uint32_t i = 0; i < gpuCount; i++) {

            PhysicalDevice physicalDevice(_instance, i, {});
            physicalDevice.printDetails();
         }
      }
#endif

      _physicalDevice = new PhysicalDevice(_instance, selectedDevice, _enabledPhysicalDeviceExtensions);
      if (!physicalDeviceAcceptable())
      {
         delete _physicalDevice;
         _physicalDevice = nullptr;

         return false;
      }

      // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
      enableFeatures();

      // Vulkan device creation
      // This is handled by a separate class that gets a logical device representation
      // and encapsulates functions related to a device
      _device = new Device(_physicalDevice, deviceCreatepNextChain);

      // Find a suitable depth format
      VkBool32 validDepthFormat = _physicalDevice->getSupportedDepthFormat(_depthFormat);
      assert(validDepthFormat);

      if (_useSwapChainRendering)
      {
         _swapChain = new SwapChain(_device);
      }
      
      // Create synchronization objects
      VkSemaphoreCreateInfo semaphoreCreateInfo = vkInitaliazers::semaphoreCreateInfo();
      // Create a semaphore used to synchronize image presentation
      // Ensures that the image is displayed before we start submitting new commands to the queue
      VK_CHECK_RESULT(vkCreateSemaphore(_device->vulkanDevice(), &semaphoreCreateInfo, nullptr, &_semaphores.presentComplete));
      // Create a semaphore used to synchronize command submission
      // Ensures that the image is not presented until all commands have been submitted and executed
      VK_CHECK_RESULT(vkCreateSemaphore(_device->vulkanDevice(), &semaphoreCreateInfo, nullptr, &_semaphores.renderComplete));

      // Set up submit info structure
      // Semaphores will stay the same during application lifetime
      // Command buffer submission info is set by each example
      _submitInfo = vkInitaliazers::submitInfo();
      _submitInfo.pWaitDstStageMask = &submitPipelineStages;
      _submitInfo.waitSemaphoreCount = 1;
      _submitInfo.pWaitSemaphores = &_semaphores.presentComplete;
      _submitInfo.signalSemaphoreCount = 1;
      _submitInfo.pSignalSemaphores = &_semaphores.renderComplete;

      return true;
   }

   // GLFW Callback functions
   static void onErrorCallback(int error, const char* description)
   {
      fprintf(stderr, "GLFW Error %d: %s\n", error, description);
   }

   static void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods)
   {
      auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
      app->onKeyboard(key, scancode, action, mods);
   }

   static void mousebutton_cb(GLFWwindow* window, int button, int action, int mods)
   {
      auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseButton(button, action, mods);
   }

   static void cursorpos_cb(GLFWwindow* window, double x, double y)
   {
      auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseMotion(static_cast<int>(x), static_cast<int>(y));
   }

   static void scroll_cb(GLFWwindow* window, double x, double y)
   {
      auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseWheel(static_cast<int>(y));
   }

   static void framebuffersize_cb(GLFWwindow* window, int w, int h)
   {
      auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
      app->onFramebufferSize(w, h);
   }

   static void char_cb(GLFWwindow* window, unsigned int key)
   {

   }

   static void drop_cb(GLFWwindow* window, int count, const char** paths)
   {
      std::vector<std::string> filesDropped;
      for (int i = 0; i < count; ++i)
      {
         if (paths[i])
         {
            filesDropped.push_back(paths[i]);
         }
      }
      if (filesDropped.size()>0)
      {
         auto app = reinterpret_cast<PlatformApplication*>(glfwGetWindowUserPointer(window));
         app->onDrop(filesDropped);
      }
   }

   void PlatformApplication::onKeyboard(int key, int scancode, int action, int mods)
   {
      switch (action)
      {
      case GLFW_PRESS:
         switch (key)
         {
         case GLFW_KEY_P:
            _paused = !_paused;
            break;
         case GLFW_KEY_F1:
            if (_settings.overlay) {
               _uiOverlay._visible = !_uiOverlay._visible;
            }
            break;
         case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(_window, 1);
            break;
         }

         if (_camera.type == Camera::firstperson)
         {
            switch (key)
            {
            case GLFW_KEY_W:
               _camera.keys.up = true;
               break;
            case GLFW_KEY_S:
               _camera.keys.down = true;
               break;
            case GLFW_KEY_A:
               _camera.keys.left = true;
               break;
            case GLFW_KEY_D:
               _camera.keys.right = true;
               break;
            }
         }

         keyPressed(key);
         break;

      case GLFW_RELEASE:
         if (_camera.type == Camera::firstperson)
         {
            switch (key)
            {
            case GLFW_KEY_W:
               _camera.keys.up = false;
               break;
            case GLFW_KEY_S:
               _camera.keys.down = false;
               break;
            case GLFW_KEY_A:
               _camera.keys.left = false;
               break;
            case GLFW_KEY_D:
               _camera.keys.right = false;
               break;
            }
         }
         break;
      default:
         break;
      }
   }

   void PlatformApplication::onMouseButton(int button, int action, int mods)
   {
      (void)mods;

      double x, y;
      glfwGetCursorPos(_window, &x, &y);

      if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
      {
         _mousePos = glm::vec2((float)x, (float)y);
         _mouseButtons.left = true;
      }
      if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
      {
         _mousePos = glm::vec2((float)x, (float)y);
         _mouseButtons.right = true;
      }
      if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
      {
         _mousePos = glm::vec2((float)x, (float)y);
         _mouseButtons.middle = true;
      }

      if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
      {
         _mouseButtons.left = false;
      }
      if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
      {
         _mouseButtons.right = false;
      }
      if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
      {
         _mouseButtons.middle = false;
      }
   }

   void PlatformApplication::onMouseMotion(int x, int y)
   {
      handleMouseMove(x, y);
   }

   void PlatformApplication::onMouseWheel(int delta)
   {
      short wheelDelta = delta;
      _camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta));
      viewUpdated = true;
   }

   void PlatformApplication::onFramebufferSize(int w, int h)
   {
      if ((_prepared && w != 0 && h != 0))
      {
         destWidth = w;
         destHeight = h;
         windowResize();
      }
   }

   void PlatformApplication::onDrop(const std::vector<std::string>& filesDropped)
   {
      // no op
   }

   void PlatformApplication::setupGlfwCallbacks(GLFWwindow* window)
   {
      glfwSetKeyCallback(window, &key_cb);
      glfwSetMouseButtonCallback(window, &mousebutton_cb);
      glfwSetCursorPosCallback(window, &cursorpos_cb);
      glfwSetScrollCallback(window, &scroll_cb);
      
      glfwSetCharCallback(window, &char_cb);
      glfwSetFramebufferSizeCallback(window, &framebuffersize_cb);
      glfwSetDropCallback(window, &drop_cb);
   }

   GLFWwindow* PlatformApplication::setupWindow()
   {
      //if (settings.fullscreen)

      // Setup GLFW window
      glfwSetErrorCallback(onErrorCallback);
      if (!glfwInit())
      {
         return 0;
      }
      if (_useSwapChainRendering)
      {
         glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      }
      _window = glfwCreateWindow(_width, _height, getWindowTitle().c_str(), nullptr, nullptr);

      // Setup Vulkan
      if (!glfwVulkanSupported())
      {
         printf("GLFW: Vulkan Not Supported\n");
      }

      glfwSetWindowUserPointer(_window, this);
      setupGlfwCallbacks(_window);

      return _window;
   }

   void PlatformApplication::viewChanged() {}

   void PlatformApplication::keyPressed(uint32_t) {}

   void PlatformApplication::mouseMoved(double x, double y, bool& handled) {}

   void PlatformApplication::buildCommandBuffers() {}

   void PlatformApplication::createSynchronizationPrimitives()
   {
      // Wait fences to sync command buffer access
      VkFenceCreateInfo fenceCreateInfo = vkInitaliazers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
      _waitFences.resize(_drawCommandBuffers.size());
      for (auto& fence : _waitFences) {
         VK_CHECK_RESULT(vkCreateFence(_device->vulkanDevice(), &fenceCreateInfo, nullptr, &fence));
      }
   }

   void PlatformApplication::createCommandPool()
   {
      VkCommandPoolCreateInfo cmdPoolInfo = {};
      cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

      // If no swap chain, we are using gl to render, so use the family index with all of graphics+compute+transfer bits
      cmdPoolInfo.queueFamilyIndex = (_useSwapChainRendering) ? _swapChain->presentationQueueFamilyIndex()
         : _device->physicalDevice()->queueFamilyIndexWithFlags(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

      cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      VK_CHECK_RESULT(vkCreateCommandPool(_device->vulkanDevice(), &cmdPoolInfo, nullptr, &_commandPool));
   }

   void PlatformApplication::setupMultiSampleColor()
   {
      if (_sampleCount == 1)
      {
         return;
      }
      VkFormat colorFormat = _swapChain ? _swapChain->colorFormat() : _colorFormatExternalRendering;
      // Image will only be used as a transient target
      VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      _multiSampledColorImage = new StorageImage(_device
         , colorFormat, _width, _height
         , usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
         , VK_IMAGE_TILING_OPTIMAL, _sampleCount);
   }

   void PlatformApplication::setupDepthStencil()
   {
      VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      if (_sampleCount > 1)
      {
         // Image will only be used as a transient target
         usageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
      }

      _depthStencilImage = new StorageImage(_device
         , _depthFormat, _width, _height
         , usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
         , VK_IMAGE_TILING_OPTIMAL, _sampleCount);
   }

   void PlatformApplication::setupFrameBuffer()
   {
      if (_dynamicRendering)
      {
         return;
      }
      
      std::vector<VkImageView> attachments;
      int swapChainAttachmentIndex = 0;

      if (_sampleCount > 1)
      {
         attachments.resize(3, {});
         swapChainAttachmentIndex = 1;

         // Depth/Stencil attachment is the same for all frame buffers
         attachments[0] = _multiSampledColorImage->vulkanImageView();
         attachments[2] = _depthStencilImage->vulkanImageView();
      }
      else
      {
         attachments.resize(2, {});
         swapChainAttachmentIndex = 0;
         // Depth/Stencil attachment is the same for all frame buffers
         attachments[1] = _depthStencilImage->vulkanImageView();
      }

      VkFramebufferCreateInfo frameBufferCreateInfo = vkInitaliazers::framebufferCreateInfo();
      frameBufferCreateInfo.renderPass = _renderPass->vulkanRenderPass();
      frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      frameBufferCreateInfo.pAttachments = attachments.data();
      frameBufferCreateInfo.width = _width;
      frameBufferCreateInfo.height = _height;
      frameBufferCreateInfo.layers = 1;

      // Create frame buffers for every swap chain image
      _frameBuffers.resize(_swapChain->imageCount());
      for (uint32_t i = 0; i < _frameBuffers.size(); i++)
      {
         attachments[swapChainAttachmentIndex] = _swapChain->imageView(i);
         VK_CHECK_RESULT(vkCreateFramebuffer(_device->vulkanDevice(), &frameBufferCreateInfo, nullptr, &_frameBuffers[i]));
      }
   }

   void PlatformApplication::destroyFrameBuffers(void)
   {
      for (uint32_t i = 0; i < _frameBuffers.size(); i++)
      {
         vkDestroyFramebuffer(_device->vulkanDevice(), _frameBuffers[i], nullptr);
      }
   }

   void PlatformApplication::setupRenderPass()
   {
      if (_dynamicRendering)
      {
         return;
      }
      
      _renderPass = new genesis::RenderPass(_device, _swapChain->colorFormat(), _depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, _sampleCount);
   }

   bool PlatformApplication::physicalDeviceAcceptable() const
   {
      return true;
   }
   
   void PlatformApplication::enableFeatures() 
   {
      // no op
   }

   void PlatformApplication::windowResize()
   {
      if (!_prepared)
      {
         return;
      }
      _prepared = false;

      if (_width == destWidth && _height == destHeight)
      {
         _prepared = true;
         viewChanged();
         return;
      }

      // Ensure all operations on the device have been finished before destroying resources
      vkDeviceWaitIdle(_device->vulkanDevice());

      // Recreate swap chain
      _width = destWidth;
      _height = destHeight;
      setupSwapChain();

      destroyMultiSampleColor();
      setupMultiSampleColor();

      destroyColor();
      setupColor();

      destroyDepthStencil();
      setupDepthStencil();

      if (_dynamicRendering == false)
      {
         destroyFrameBuffers();
         setupFrameBuffer();
      }
      if ((_width > 0.0f) && (_height > 0.0f)) 
      {
         if (_settings.overlay) 
         {
            _uiOverlay.resize(_width, _height);
         }
      }

      // Command buffers need to be recreated as they may store
      // references to the recreated frame buffer
      destroyCommandBuffers();
      createCommandBuffers();
      buildCommandBuffers();

      vkDeviceWaitIdle(_device->vulkanDevice());

      if ((_width > 0.0f) && (_height > 0.0f)) {
         _camera.updateAspectRatio((float)_width / (float)_height);
      }

      // Notify derived class
      windowResized();
      viewChanged();

      _prepared = true;
   }

   void PlatformApplication::handleMouseMove(int32_t x, int32_t y)
   {
      int32_t dx = (int32_t)_mousePos.x - x;
      int32_t dy = (int32_t)_mousePos.y - y;
      dy = -dy;

      bool handled = false;

      if (_settings.overlay) {
         ImGuiIO& io = ImGui::GetIO();
         handled = io.WantCaptureMouse;
      }
      mouseMoved((float)x, (float)y, handled);

      if (handled) {
         _mousePos = glm::vec2((float)x, (float)y);
         return;
      }

      if (_mouseButtons.left) {
         _camera.rotate(glm::vec3(dy * _camera.rotationSpeed, -dx * _camera.rotationSpeed, 0.0f));
         viewUpdated = true;
      }
      if (_mouseButtons.right) {
         _camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
         viewUpdated = true;
      }
      if (_mouseButtons.middle) {
         _camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
         viewUpdated = true;
      }
      _mousePos = glm::vec2((float)x, (float)y);
   }

   void PlatformApplication::windowResized() {}

   void PlatformApplication::initSwapchain()
   {
#if defined(VK_USE_PLATFORM_GLFW)
      _swapChain->initSurface(_window);
#elif defined(_WIN32)
      swapChain.initSurface(windowInstance, _window);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
      swapChain.initSurface(androidApp->window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
      swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      swapChain.initSurface(connection, _window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
      swapChain.initSurface(_width, _height);
#endif
   }

   void PlatformApplication::setupSwapChain()
   {
      _swapChain->create(&_width, &_height, _settings.vsync);
   }

   void PlatformApplication::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
   {
      // no op
   }

   void PlatformApplication::destroyDepthStencil(void)
   {
      delete _depthStencilImage;
      _depthStencilImage = nullptr;
   }

   void PlatformApplication::destroyMultiSampleColor(void)
   {
      delete _multiSampledColorImage;
      _multiSampledColorImage = nullptr;
   }

   VkFormat PlatformApplication::colorFormat() const
   {
      return (_useSwapChainRendering) ? _swapChain->colorFormat() : _colorFormatExternalRendering;
   }

   void PlatformApplication::setupColor(void)
   {
      if (_useSwapChainRendering)
      {
         return;
      }
      // This image can be rendered directly to -> VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      // 
      // This is read by glDrawVkImageNV to draw using gl. So it is source -> VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      // 
      // This can be blitted to. For example, if there is post processing or if we are doing ray tracing
      // So it is also destination-> VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      // 
      // Sample count is always 1, because the multisample color will be resolved into this image
      VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      _colorImage = new StorageImage(_device
         , colorFormat(), _width, _height
         , usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
         , VK_IMAGE_TILING_OPTIMAL, 1);
   }

   void PlatformApplication::destroyColor(void)
   {
      if (_useSwapChainRendering)
      {
         return;
      }
      delete _colorImage;
      _colorImage = nullptr;
   }
}