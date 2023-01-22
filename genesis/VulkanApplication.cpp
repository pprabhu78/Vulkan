/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanApplication.h"
#include "RenderPass.h"
#include "Instance.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "VulkanInitializers.h"
#include "VulkanExtensions.h"


#ifdef VK_USE_PLATFORM_GLFW
#if _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "../external/glfw/include/GLFW/glfw3.h"
#include "../external/glfw/include/GLFW/glfw3native.h"
#endif

namespace genesis
{
   std::vector<const char*> VulkanApplication::args;

   VkResult VulkanApplication::createInstance(bool enableValidation)
   {
      this->_settings.validation = enableValidation;

      // Validation can also be forced via a define
#if defined(_VALIDATION)
      this->_settings.validation = true;
#endif

      _instance = new ApiInstance(_name, _enabledInstanceExtensions, apiVersion, this->_settings.validation);
      return _instance->creationStatus();
   }

   void VulkanApplication::renderFrame()
   {
      VulkanApplication::prepareFrame();
      _submitInfo.commandBufferCount = 1;
      _submitInfo.pCommandBuffers = &_drawCommandBuffers[_currentFrameBufferIndex];
      VK_CHECK_RESULT(vkQueueSubmit(_device->graphicsQueue(), 1, &_submitInfo, VK_NULL_HANDLE));
      VulkanApplication::submitFrame();
   }

   std::string VulkanApplication::getWindowTitle()
   {
      std::string device(_physicalDevice->physicalDeviceProperties().deviceName);
      std::string windowTitle;
      windowTitle = _title + " - " + device;
      if (!_settings.overlay) {
         windowTitle += " - " + std::to_string(_frameCounter) + " fps";
      }
      return windowTitle;
   }

   void VulkanApplication::createCommandBuffers()
   {
      // Create one command buffer for each swap chain image and reuse for rendering
      _drawCommandBuffers.resize(_swapChain->imageCount());

      VkCommandBufferAllocateInfo cmdBufAllocateInfo =
         VulkanInitializers::commandBufferAllocateInfo(
            _commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<uint32_t>(_drawCommandBuffers.size()));

      VK_CHECK_RESULT(vkAllocateCommandBuffers(_device->vulkanDevice(), &cmdBufAllocateInfo, _drawCommandBuffers.data()));
   }

   void VulkanApplication::destroyCommandBuffers()
   {
      if (_drawCommandBuffers.empty())
      {
         return;
      }
      vkFreeCommandBuffers(_device->vulkanDevice(), _commandPool, static_cast<uint32_t>(_drawCommandBuffers.size()), _drawCommandBuffers.data());
   }

   std::string VulkanApplication::getAssetsPath() const
   {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
      return "";
#elif defined(VK_EXAMPLE_DATA_DIR)
      return VK_EXAMPLE_DATA_DIR;
#else
      return "./../data/";
#endif
   }

   std::string VulkanApplication::getShadersPath() const
   {
      return getAssetsPath() + "shaders/" + shaderDir + "/";
   }

   void VulkanApplication::createPipelineCache()
   {
      VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
      pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
      VK_CHECK_RESULT(vkCreatePipelineCache(_device->vulkanDevice(), &pipelineCacheCreateInfo, nullptr, &pipelineCache));
   }

   void VulkanApplication::prepare()
   {
      if (_device->enableDebugMarkers()) {
         genesis::debugmarker::setup(_device->vulkanDevice());
      }
      initSwapchain();
      createCommandPool();
      setupSwapChain();
      createCommandBuffers();
      createSynchronizationPrimitives();
      setupDepthStencil();
      setupRenderPass();
      createPipelineCache();
      setupFrameBuffer();
      _settings.overlay = _settings.overlay && (!benchmark.active);

      UIOverlay.device = _device;
      UIOverlay._shaders.push_back(loadShader(getShadersPath() + "genesis/uioverlay.vert.spv", genesis::ST_VERTEX_SHADER));
      UIOverlay._shaders.push_back(loadShader(getShadersPath() + "genesis/uioverlay.frag.spv", genesis::ST_FRAGMENT_SHADER));
      UIOverlay.prepareResources();
      UIOverlay.preparePipeline(pipelineCache, _renderPass->vulkanRenderPass());
   }

   genesis::Shader* VulkanApplication::loadShader(std::string fileName, genesis::ShaderType stage)
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

   void VulkanApplication::nextFrame()
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
      frameTimer = (float)tDiff / 1000.0f;
      _camera.update(frameTimer);
      if (_camera.moving())
      {
         viewUpdated = true;
      }
      // Convert to clamped timer value
      if (!_paused)
      {
         _timer += _timerSpeed * frameTimer;
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
            glfwSetWindowTitle(window, windowTitle.c_str());
#else
            SetWindowText(window, windowTitle.c_str());
#endif
         }
#endif
         _frameCounter = 0;
         _lastTimestamp = tEnd;
      }
      // TODO: Cap UI overlay update rates
      updateOverlay();
   }

   static bool IsMinimized(GLFWwindow* window)
   {
      int w, h;
      glfwGetWindowSize(window, &w, &h);
      bool minimized = (w == 0 && h == 0);
      return minimized;
   }

   void VulkanApplication::renderLoop()
   {
      if (benchmark.active) {
         benchmark.run([=] { render(); }, _physicalDevice->physicalDeviceProperties());
         vkDeviceWaitIdle(_device->vulkanDevice());
         if (benchmark.filename != "") {
            benchmark.saveResults();
         }
         return;
      }

      destWidth = _width;
      destHeight = _height;
      _lastTimestamp = std::chrono::high_resolution_clock::now();

      bool quitMessageReceived = false;
      while (!glfwWindowShouldClose(window))
      {
         glfwPollEvents();
#if 0
         if (msg.message == WM_QUIT)
         {
            quitMessageReceived = true;
            break;
         }
#endif

         if (_prepared && !IsMinimized(window)) {
            nextFrame();
         }
      }

      // Flush device to make sure all resources can be freed
      if (_device->vulkanDevice() != VK_NULL_HANDLE) {
         vkDeviceWaitIdle(_device->vulkanDevice());
      }

      glfwDestroyWindow(window);
      glfwTerminate();
   }

   void VulkanApplication::updateOverlay()
   {
      if (!_settings.overlay)
         return;

      ImGuiIO& io = ImGui::GetIO();

      io.DisplaySize = ImVec2((float)_width, (float)_height);
      io.DeltaTime = frameTimer;

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


      ImGui::PushItemWidth(110.0f * UIOverlay.scale);
      OnUpdateUIOverlay(&UIOverlay);
      ImGui::PopItemWidth();


      ImGui::End();
      ImGui::PopStyleVar();
      ImGui::Render();

      if (UIOverlay.update() || UIOverlay.updated) {
         buildCommandBuffers();
         UIOverlay.updated = false;
      }

   }

   void VulkanApplication::drawUI(const VkCommandBuffer commandBuffer)
   {
      if (_settings.overlay) {
         const VkViewport viewport = VulkanInitializers::viewport((float)_width, (float)_height, 0.0f, 1.0f, false);
         const VkRect2D scissor = VulkanInitializers::rect2D(_width, _height, 0, 0);
         vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
         vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

         UIOverlay.draw(commandBuffer);
      }
   }

   void VulkanApplication::prepareFrame()
   {
      // Acquire the next image from the swap chain
      VkResult result = _swapChain->acquireNextImage(_currentFrameBufferIndex, _semaphores.presentComplete);
      // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
      if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
         windowResize();
      }
      else {
         VK_CHECK_RESULT(result);
      }
   }

   void VulkanApplication::submitFrame()
   {
      VkResult result = _swapChain->queuePresent(_device->graphicsQueue(), _currentFrameBufferIndex, _semaphores.renderComplete);
      if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
         if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Swap chain is no longer compatible with the surface and needs to be recreated
            windowResize();
            return;
         }
         else {
            VK_CHECK_RESULT(result);
         }
      }
      VK_CHECK_RESULT(vkQueueWaitIdle(_device->graphicsQueue()));
   }

   VulkanApplication::VulkanApplication(bool enableValidation)
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
      commandLineParser.parse(args);
      if (commandLineParser.isSet("help"))
      {
         commandLineParser.printHelp();
         std::cin.get();
         exit(0);
      }
      if (commandLineParser.isSet("validation")) {
         _settings.validation = true;
      }
      if (commandLineParser.isSet("vsync")) {
         _settings.vsync = true;
      }
      if (commandLineParser.isSet("height")) {
         _height = commandLineParser.getValueAsInt("height", _width);
      }
      if (commandLineParser.isSet("width")) {
         _width = commandLineParser.getValueAsInt("width", _width);
      }
      if (commandLineParser.isSet("fullscreen")) {
         _settings.fullscreen = true;
      }
      if (commandLineParser.isSet("shaders")) {
         std::string value = commandLineParser.getValueAsString("shaders", "glsl");
         if ((value != "glsl") && (value != "hlsl")) {
            std::cerr << "Shader type must be one of 'glsl' or 'hlsl'\n";
         }
         else {
            shaderDir = value;
         }
      }
      if (commandLineParser.isSet("benchmark")) {
         benchmark.active = true;
      }
      if (commandLineParser.isSet("benchmarkwarmup")) {
         benchmark.warmup = commandLineParser.getValueAsInt("benchmarkwarmup", benchmark.warmup);
      }
      if (commandLineParser.isSet("benchmarkruntime")) {
         benchmark.duration = commandLineParser.getValueAsInt("benchmarkruntime", benchmark.duration);
      }
      if (commandLineParser.isSet("benchmarkresultfile")) {
         benchmark.filename = commandLineParser.getValueAsString("benchmarkresultfile", benchmark.filename);
      }
      if (commandLineParser.isSet("benchmarkresultframes")) {
         benchmark.outputFrameTimes = true;
      }
      if (commandLineParser.isSet("benchmarkframes")) {
         benchmark.outputFrames = commandLineParser.getValueAsInt("benchmarkframes", benchmark.outputFrames);
      }
   }

   VulkanApplication::~VulkanApplication()
   {
      delete _swapChain;

      if (descriptorPool != VK_NULL_HANDLE)
      {
         vkDestroyDescriptorPool(_device->vulkanDevice(), descriptorPool, nullptr);
      }
      destroyCommandBuffers();
      delete _renderPass;
      for (uint32_t i = 0; i < _frameBuffers.size(); i++)
      {
         vkDestroyFramebuffer(_device->vulkanDevice(), _frameBuffers[i], nullptr);
      }

      for (Shader* shader : _shaders)
      {
         delete shader;
      }
      vkDestroyImageView(_device->vulkanDevice(), _depthStencil.view, nullptr);
      vkDestroyImage(_device->vulkanDevice(), _depthStencil.image, nullptr);
      vkFreeMemory(_device->vulkanDevice(), _depthStencil.mem, nullptr);

      vkDestroyPipelineCache(_device->vulkanDevice(), pipelineCache, nullptr);

      vkDestroyCommandPool(_device->vulkanDevice(), _commandPool, nullptr);

      vkDestroySemaphore(_device->vulkanDevice(), _semaphores.presentComplete, nullptr);
      vkDestroySemaphore(_device->vulkanDevice(), _semaphores.renderComplete, nullptr);
      for (auto& fence : _waitFences) {
         vkDestroyFence(_device->vulkanDevice(), fence, nullptr);
      }

      UIOverlay.freeResources();

      delete _device;

      delete _instance;

   }

   bool VulkanApplication::initVulkan()
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
      if (commandLineParser.isSet("gpuselection")) {
         uint32_t index = commandLineParser.getValueAsInt("gpuselection", 0);
         if (index > gpuCount - 1) {
            std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
         }
         else {
            selectedDevice = index;
         }
      }
      if (commandLineParser.isSet("gpulist")) {
         std::cout << "Available Vulkan devices" << "\n";
         for (uint32_t i = 0; i < gpuCount; i++) {

            PhysicalDevice physicalDevice(_instance, i, {});
            physicalDevice.printDetails();
         }
      }
#endif

      _physicalDevice = new PhysicalDevice(_instance, selectedDevice, _enabledPhysicalDeviceExtensions);

      // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
      enableFeatures();

      // Vulkan device creation
      // This is handled by a separate class that gets a logical device representation
      // and encapsulates functions related to a device
      _device = new Device(_physicalDevice, deviceCreatepNextChain);

      // Find a suitable depth format
      VkBool32 validDepthFormat = _physicalDevice->getSupportedDepthFormat(_depthFormat);
      assert(validDepthFormat);

      _swapChain = new SwapChain(_device);

      // Create synchronization objects
      VkSemaphoreCreateInfo semaphoreCreateInfo = VulkanInitializers::semaphoreCreateInfo();
      // Create a semaphore used to synchronize image presentation
      // Ensures that the image is displayed before we start submitting new commands to the queue
      VK_CHECK_RESULT(vkCreateSemaphore(_device->vulkanDevice(), &semaphoreCreateInfo, nullptr, &_semaphores.presentComplete));
      // Create a semaphore used to synchronize command submission
      // Ensures that the image is not presented until all commands have been submitted and executed
      VK_CHECK_RESULT(vkCreateSemaphore(_device->vulkanDevice(), &semaphoreCreateInfo, nullptr, &_semaphores.renderComplete));

      // Set up submit info structure
      // Semaphores will stay the same during application lifetime
      // Command buffer submission info is set by each example
      _submitInfo = VulkanInitializers::submitInfo();
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
      auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
      app->onKeyboard(key, scancode, action, mods);
   }

   static void mousebutton_cb(GLFWwindow* window, int button, int action, int mods)
   {
      auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseButton(button, action, mods);
   }

   static void cursorpos_cb(GLFWwindow* window, double x, double y)
   {
      auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseMotion(static_cast<int>(x), static_cast<int>(y));
   }

   static void scroll_cb(GLFWwindow* window, double x, double y)
   {
      auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
      app->onMouseWheel(static_cast<int>(y));
   }

   static void framebuffersize_cb(GLFWwindow* window, int w, int h)
   {
      auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
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
         auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
         app->onDrop(filesDropped);
      }
   }

   void VulkanApplication::onKeyboard(int key, int scancode, int action, int mods)
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
               UIOverlay.visible = !UIOverlay.visible;
            }
            break;
         case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, 1);
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

   void VulkanApplication::onMouseButton(int button, int action, int mods)
   {
      (void)mods;

      double x, y;
      glfwGetCursorPos(window, &x, &y);

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

   void VulkanApplication::onMouseMotion(int x, int y)
   {
      handleMouseMove(x, y);
   }

   void VulkanApplication::onMouseWheel(int delta)
   {
      short wheelDelta = delta;
      _camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta));
      viewUpdated = true;
   }

   void VulkanApplication::onFramebufferSize(int w, int h)
   {
      if ((_prepared && w != 0 && h != 0))
      {
         destWidth = w;
         destHeight = h;
         windowResize();
      }
   }

   void VulkanApplication::onDrop(const std::vector<std::string>& filesDropped)
   {
      // no op
   }

   static void setupGlfwCallbacks(GLFWwindow* window)
   {
      glfwSetKeyCallback(window, &key_cb);
      glfwSetMouseButtonCallback(window, &mousebutton_cb);
      glfwSetCursorPosCallback(window, &cursorpos_cb);
      glfwSetScrollCallback(window, &scroll_cb);
      
      glfwSetCharCallback(window, &char_cb);
      glfwSetFramebufferSizeCallback(window, &framebuffersize_cb);
      glfwSetDropCallback(window, &drop_cb);
   }

   GLFWwindow* VulkanApplication::setupWindow()
   {
      //if (settings.fullscreen)

      // Setup GLFW window
      glfwSetErrorCallback(onErrorCallback);
      if (!glfwInit())
      {
         return 0;
      }
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      window = glfwCreateWindow(_width, _height, getWindowTitle().c_str(), nullptr, nullptr);

      // Setup Vulkan
      if (!glfwVulkanSupported())
      {
         printf("GLFW: Vulkan Not Supported\n");
      }

      glfwSetWindowUserPointer(window, this);
      setupGlfwCallbacks(window);

      return window;
   }

   void VulkanApplication::viewChanged() {}

   void VulkanApplication::keyPressed(uint32_t) {}

   void VulkanApplication::mouseMoved(double x, double y, bool& handled) {}

   void VulkanApplication::buildCommandBuffers() {}

   void VulkanApplication::createSynchronizationPrimitives()
   {
      // Wait fences to sync command buffer access
      VkFenceCreateInfo fenceCreateInfo = VulkanInitializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
      _waitFences.resize(_drawCommandBuffers.size());
      for (auto& fence : _waitFences) {
         VK_CHECK_RESULT(vkCreateFence(_device->vulkanDevice(), &fenceCreateInfo, nullptr, &fence));
      }
   }

   void VulkanApplication::createCommandPool()
   {
      VkCommandPoolCreateInfo cmdPoolInfo = {};
      cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      cmdPoolInfo.queueFamilyIndex = _swapChain->presentationQueueFamilyIndex();
      cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      VK_CHECK_RESULT(vkCreateCommandPool(_device->vulkanDevice(), &cmdPoolInfo, nullptr, &_commandPool));
   }

   void VulkanApplication::setupDepthStencil()
   {
      VkImageCreateInfo imageCI{};
      imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageCI.imageType = VK_IMAGE_TYPE_2D;
      imageCI.format = _depthFormat;
      imageCI.extent = { _width, _height, 1 };
      imageCI.mipLevels = 1;
      imageCI.arrayLayers = 1;
      imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

      VK_CHECK_RESULT(vkCreateImage(_device->vulkanDevice(), &imageCI, nullptr, &_depthStencil.image));
      VkMemoryRequirements memReqs{};
      vkGetImageMemoryRequirements(_device->vulkanDevice(), _depthStencil.image, &memReqs);

      VkMemoryAllocateInfo memAllloc{};
      memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memAllloc.allocationSize = memReqs.size;
      memAllloc.memoryTypeIndex = _physicalDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memAllloc, nullptr, &_depthStencil.mem));
      VK_CHECK_RESULT(vkBindImageMemory(_device->vulkanDevice(), _depthStencil.image, _depthStencil.mem, 0));

      VkImageViewCreateInfo imageViewCI{};
      imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewCI.image = _depthStencil.image;
      imageViewCI.format = _depthFormat;
      imageViewCI.subresourceRange.baseMipLevel = 0;
      imageViewCI.subresourceRange.levelCount = 1;
      imageViewCI.subresourceRange.baseArrayLayer = 0;
      imageViewCI.subresourceRange.layerCount = 1;
      imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
      if (_depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
         imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      VK_CHECK_RESULT(vkCreateImageView(_device->vulkanDevice(), &imageViewCI, nullptr, &_depthStencil.view));
   }

   void VulkanApplication::setupFrameBuffer()
   {
      VkImageView attachments[2];

      // Depth/Stencil attachment is the same for all frame buffers
      attachments[1] = _depthStencil.view;

      VkFramebufferCreateInfo frameBufferCreateInfo = {};
      frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      frameBufferCreateInfo.pNext = NULL;
      frameBufferCreateInfo.renderPass = _renderPass->vulkanRenderPass();
      frameBufferCreateInfo.attachmentCount = 2;
      frameBufferCreateInfo.pAttachments = attachments;
      frameBufferCreateInfo.width = _width;
      frameBufferCreateInfo.height = _height;
      frameBufferCreateInfo.layers = 1;

      // Create frame buffers for every swap chain image
      _frameBuffers.resize(_swapChain->imageCount());
      for (uint32_t i = 0; i < _frameBuffers.size(); i++)
      {
         attachments[0] = _swapChain->imageView(i);
         VK_CHECK_RESULT(vkCreateFramebuffer(_device->vulkanDevice(), &frameBufferCreateInfo, nullptr, &_frameBuffers[i]));
      }
   }

   void VulkanApplication::setupRenderPass()
   {
      _renderPass = new genesis::RenderPass(_device, _swapChain->colorFormat(), _depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR);
   }

   void VulkanApplication::enableFeatures() 
   {
      // no op
   }

   void VulkanApplication::windowResize()
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

      // Recreate the frame buffers
      vkDestroyImageView(_device->vulkanDevice(), _depthStencil.view, nullptr);
      vkDestroyImage(_device->vulkanDevice(), _depthStencil.image, nullptr);
      vkFreeMemory(_device->vulkanDevice(), _depthStencil.mem, nullptr);
      setupDepthStencil();
      for (uint32_t i = 0; i < _frameBuffers.size(); i++) {
         vkDestroyFramebuffer(_device->vulkanDevice(), _frameBuffers[i], nullptr);
      }
      setupFrameBuffer();

      if ((_width > 0.0f) && (_height > 0.0f)) {
         if (_settings.overlay) {
            UIOverlay.resize(_width, _height);
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

   void VulkanApplication::handleMouseMove(int32_t x, int32_t y)
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

   void VulkanApplication::windowResized() {}

   void VulkanApplication::initSwapchain()
   {
#if defined(VK_USE_PLATFORM_GLFW)
      _swapChain->initSurface(window);
#elif defined(_WIN32)
      swapChain.initSurface(windowInstance, window);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
      swapChain.initSurface(androidApp->window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
      swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      swapChain.initSurface(connection, window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
      swapChain.initSurface(_width, _height);
#endif
   }

   void VulkanApplication::setupSwapChain()
   {
      _swapChain->create(&_width, &_height, _settings.vsync);
   }

   void VulkanApplication::OnUpdateUIOverlay(genesis::UIOverlay* overlay)
   {
      // no op
   }
}