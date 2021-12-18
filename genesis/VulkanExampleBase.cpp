/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanExampleBase.h"
#include "RenderPass.h"
#include "Instance.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VulkanDebug.h"
#include "VulkanInitializers.h"

namespace genesis
{
   std::vector<const char*> VulkanExampleBase::args;

   VkResult VulkanExampleBase::createInstance(bool enableValidation)
   {
      this->settings.validation = enableValidation;

      // Validation can also be forced via a define
#if defined(_VALIDATION)
      this->settings.validation = true;
#endif

      _instance = new Instance(name, _enabledInstanceExtensions, apiVersion, this->settings.validation);
      return _instance->creationStatus();
   }

   void VulkanExampleBase::renderFrame()
   {
      VulkanExampleBase::prepareFrame();
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
      VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
      VulkanExampleBase::submitFrame();
   }

   std::string VulkanExampleBase::getWindowTitle()
   {
      std::string device(_physicalDevice->physicalDeviceProperties().deviceName);
      std::string windowTitle;
      windowTitle = title + " - " + device;
      if (!settings.overlay) {
         windowTitle += " - " + std::to_string(frameCounter) + " fps";
      }
      return windowTitle;
   }

   void VulkanExampleBase::createCommandBuffers()
   {
      // Create one command buffer for each swap chain image and reuse for rendering
      drawCmdBuffers.resize(swapChain.imageCount);

      VkCommandBufferAllocateInfo cmdBufAllocateInfo =
         VulkanInitializers::commandBufferAllocateInfo(
            cmdPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<uint32_t>(drawCmdBuffers.size()));

      VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
   }

   void VulkanExampleBase::destroyCommandBuffers()
   {
      if (drawCmdBuffers.empty())
      {
         return;
      }
      vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
   }

   std::string VulkanExampleBase::getShadersPath() const
   {
      return getAssetPath() + "shaders/" + shaderDir + "/";
   }

   void VulkanExampleBase::createPipelineCache()
   {
      VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
      pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
      VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
   }

   void VulkanExampleBase::prepare()
   {
      if (vulkanDevice->enableDebugMarkers) {
         vks::debugmarker::setup(device);
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
      settings.overlay = settings.overlay && (!benchmark.active);
      if (settings.overlay) {
         UIOverlay.device = vulkanDevice;
         UIOverlay.queue = queue;
         UIOverlay.shaders = {
         loadShader(getShadersPath() + "base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
         loadShader(getShadersPath() + "base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
         };
         UIOverlay.prepareResources();
         UIOverlay.preparePipeline(pipelineCache, _renderPass->vulkanRenderPass());
      }
   }

   VkPipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string fileName, VkShaderStageFlagBits stage)
   {
      VkPipelineShaderStageCreateInfo shaderStage = {};
      shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
      shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device);
#else
      shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
#endif
      shaderStage.pName = "main";
      assert(shaderStage.module != VK_NULL_HANDLE);
      shaderModules.push_back(shaderStage.module);
      return shaderStage;
   }

   void VulkanExampleBase::nextFrame()
   {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated)
      {
         viewUpdated = false;
         viewChanged();
      }

      render();
      frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      frameTimer = (float)tDiff / 1000.0f;
      camera.update(frameTimer);
      if (camera.moving())
      {
         viewUpdated = true;
      }
      // Convert to clamped timer value
      if (!paused)
      {
         timer += timerSpeed * frameTimer;
         if (timer > 1.0)
         {
            timer -= 1.0f;
         }
      }
      float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
      if (fpsTimer > 1000.0f)
      {
         lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
#if defined(_WIN32)
         if (!settings.overlay) {
            std::string windowTitle = getWindowTitle();
            SetWindowText(window, windowTitle.c_str());
         }
#endif
         frameCounter = 0;
         lastTimestamp = tEnd;
      }
      // TODO: Cap UI overlay update rates
      updateOverlay();
   }

   void VulkanExampleBase::renderLoop()
   {
      if (benchmark.active) {
         benchmark.run([=] { render(); }, vulkanDevice->properties);
         vkDeviceWaitIdle(device);
         if (benchmark.filename != "") {
            benchmark.saveResults();
         }
         return;
      }

      destWidth = width;
      destHeight = height;
      lastTimestamp = std::chrono::high_resolution_clock::now();

      MSG msg;
      bool quitMessageReceived = false;
      while (!quitMessageReceived) {
         while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
               quitMessageReceived = true;
               break;
            }
         }
         if (prepared && !IsIconic(window)) {
            nextFrame();
         }
      }

      // Flush device to make sure all resources can be freed
      if (device != VK_NULL_HANDLE) {
         vkDeviceWaitIdle(device);
      }
   }

   void VulkanExampleBase::updateOverlay()
   {
      if (!settings.overlay)
         return;

      ImGuiIO& io = ImGui::GetIO();

      io.DisplaySize = ImVec2((float)width, (float)height);
      io.DeltaTime = frameTimer;

      io.MousePos = ImVec2(mousePos.x, mousePos.y);
      io.MouseDown[0] = mouseButtons.left;
      io.MouseDown[1] = mouseButtons.right;

      ImGui::NewFrame();

      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
      ImGui::SetNextWindowPos(ImVec2(10, 10));
      ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
      ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
      ImGui::TextUnformatted(title.c_str());
      ImGui::TextUnformatted(_physicalDevice->physicalDeviceProperties().deviceName);
      ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);


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

   void VulkanExampleBase::drawUI(const VkCommandBuffer commandBuffer)
   {
      if (settings.overlay) {
         const VkViewport viewport = VulkanInitializers::viewport((float)width, (float)height, 0.0f, 1.0f);
         const VkRect2D scissor = VulkanInitializers::rect2D(width, height, 0, 0);
         vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
         vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

         UIOverlay.draw(commandBuffer);
      }
   }

   void VulkanExampleBase::prepareFrame()
   {
      // Acquire the next image from the swap chain
      VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
      // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
      if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
         windowResize();
      }
      else {
         VK_CHECK_RESULT(result);
      }
   }

   void VulkanExampleBase::submitFrame()
   {
      VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
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
      VK_CHECK_RESULT(vkQueueWaitIdle(queue));
   }

   VulkanExampleBase::VulkanExampleBase(bool enableValidation)
      : _device(nullptr)
   {
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
      // Check for a valid asset path
      struct stat info;
      if (stat(getAssetPath().c_str(), &info) != 0)
      {
#if defined(_WIN32)
         std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
         MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
#else
         std::cerr << "Error: Could not find asset path in " << getAssetPath() << "\n";
#endif
         exit(-1);
      }
#endif

      settings.validation = enableValidation;

      // Command line arguments
      commandLineParser.parse(args);
      if (commandLineParser.isSet("help")) {
#if defined(_WIN32)
         setupConsole("Vulkan example");
#endif
         commandLineParser.printHelp();
         std::cin.get();
         exit(0);
      }
      if (commandLineParser.isSet("validation")) {
         settings.validation = true;
      }
      if (commandLineParser.isSet("vsync")) {
         settings.vsync = true;
      }
      if (commandLineParser.isSet("height")) {
         height = commandLineParser.getValueAsInt("height", width);
      }
      if (commandLineParser.isSet("width")) {
         width = commandLineParser.getValueAsInt("width", width);
      }
      if (commandLineParser.isSet("fullscreen")) {
         settings.fullscreen = true;
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
         vks::tools::errorModeSilent = true;
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


#if defined(_WIN32)
      // Enable console if validation is active, debug message callback will output to it
      if (this->settings.validation)
      {
         setupConsole("Vulkan example");
      }
      setupDPIAwareness();
#endif
   }

   VulkanExampleBase::~VulkanExampleBase()
   {
      // Clean up Vulkan resources
      swapChain.cleanup();
      if (descriptorPool != VK_NULL_HANDLE)
      {
         vkDestroyDescriptorPool(device, descriptorPool, nullptr);
      }
      destroyCommandBuffers();
      delete _renderPass;
      for (uint32_t i = 0; i < frameBuffers.size(); i++)
      {
         vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
      }

      for (auto& shaderModule : shaderModules)
      {
         vkDestroyShaderModule(device, shaderModule, nullptr);
      }
      vkDestroyImageView(device, depthStencil.view, nullptr);
      vkDestroyImage(device, depthStencil.image, nullptr);
      vkFreeMemory(device, depthStencil.mem, nullptr);

      vkDestroyPipelineCache(device, pipelineCache, nullptr);

      vkDestroyCommandPool(device, cmdPool, nullptr);

      vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
      vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
      for (auto& fence : waitFences) {
         vkDestroyFence(device, fence, nullptr);
      }

      if (settings.overlay) {
         UIOverlay.freeResources();
      }

      delete vulkanDevice;

      delete _instance;

   }

   bool VulkanExampleBase::initVulkan()
   {
      VkResult err;

      // Vulkan instance
      err = createInstance(settings.validation);
      if (err) {
         vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), err);
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
      getEnabledFeatures();

      // Vulkan device creation
      // This is handled by a separate class that gets a logical device representation
      // and encapsulates functions related to a device
      vulkanDevice = new vks::VulkanDevice(_physicalDevice->vulkanPhysicalDevice());
      VkResult res = vulkanDevice->createLogicalDevice(_physicalDevice->enabledPhysicalDeviceFeatures(), _physicalDevice->enabledPhysicalDeviceExtensions(), deviceCreatepNextChain);
      if (res != VK_SUCCESS) {
         vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), res);
         return false;
      }
      device = vulkanDevice->logicalDevice;

      // Get a graphics queue from the device
      vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

      // Find a suitable depth format
      VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(_physicalDevice->vulkanPhysicalDevice(), &depthFormat);
      assert(validDepthFormat);

      swapChain.connect(_instance->vulkanInstance(), _physicalDevice->vulkanPhysicalDevice(), device);

      // Create synchronization objects
      VkSemaphoreCreateInfo semaphoreCreateInfo = VulkanInitializers::semaphoreCreateInfo();
      // Create a semaphore used to synchronize image presentation
      // Ensures that the image is displayed before we start submitting new commands to the queue
      VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
      // Create a semaphore used to synchronize command submission
      // Ensures that the image is not presented until all commands have been submitted and executed
      VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

      // Set up submit info structure
      // Semaphores will stay the same during application lifetime
      // Command buffer submission info is set by each example
      submitInfo = VulkanInitializers::submitInfo();
      submitInfo.pWaitDstStageMask = &submitPipelineStages;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &semaphores.presentComplete;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &semaphores.renderComplete;

      return true;
   }

   // Win32 : Sets up a console window and redirects standard output to it
   void VulkanExampleBase::setupConsole(std::string title)
   {
      AllocConsole();
      AttachConsole(GetCurrentProcessId());
      FILE* stream;
      freopen_s(&stream, "CONIN$", "r", stdin);
      freopen_s(&stream, "CONOUT$", "w+", stdout);
      freopen_s(&stream, "CONOUT$", "w+", stderr);
      SetConsoleTitle(TEXT(title.c_str()));
   }

   void VulkanExampleBase::setupDPIAwareness()
   {
      typedef HRESULT* (__stdcall* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

      HMODULE shCore = LoadLibraryA("Shcore.dll");
      if (shCore)
      {
         SetProcessDpiAwarenessFunc setProcessDpiAwareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

         if (setProcessDpiAwareness != nullptr)
         {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
         }

         FreeLibrary(shCore);
      }
   }

   HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
   {
      this->windowInstance = hinstance;

      WNDCLASSEX wndClass;

      wndClass.cbSize = sizeof(WNDCLASSEX);
      wndClass.style = CS_HREDRAW | CS_VREDRAW;
      wndClass.lpfnWndProc = wndproc;
      wndClass.cbClsExtra = 0;
      wndClass.cbWndExtra = 0;
      wndClass.hInstance = hinstance;
      wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
      wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
      wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
      wndClass.lpszMenuName = NULL;
      wndClass.lpszClassName = name.c_str();
      wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

      if (!RegisterClassEx(&wndClass))
      {
         std::cout << "Could not register window class!\n";
         fflush(stdout);
         exit(1);
      }

      int screenWidth = GetSystemMetrics(SM_CXSCREEN);
      int screenHeight = GetSystemMetrics(SM_CYSCREEN);

      if (settings.fullscreen)
      {
         if ((width != (uint32_t)screenWidth) && (height != (uint32_t)screenHeight))
         {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth = width;
            dmScreenSettings.dmPelsHeight = height;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
               if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
               {
                  settings.fullscreen = false;
               }
               else
               {
                  return nullptr;
               }
            }
            screenWidth = width;
            screenHeight = height;
         }

      }

      DWORD dwExStyle;
      DWORD dwStyle;

      if (settings.fullscreen)
      {
         dwExStyle = WS_EX_APPWINDOW;
         dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }
      else
      {
         dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
         dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }

      RECT windowRect;
      windowRect.left = 0L;
      windowRect.top = 0L;
      windowRect.right = settings.fullscreen ? (long)screenWidth : (long)width;
      windowRect.bottom = settings.fullscreen ? (long)screenHeight : (long)height;

      AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

      std::string windowTitle = getWindowTitle();
      window = CreateWindowEx(0,
         name.c_str(),
         windowTitle.c_str(),
         dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
         0,
         0,
         windowRect.right - windowRect.left,
         windowRect.bottom - windowRect.top,
         NULL,
         NULL,
         hinstance,
         NULL);

      if (!settings.fullscreen)
      {
         // Center on screen
         uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
         uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
         SetWindowPos(window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
      }

      if (!window)
      {
         printf("Could not create window!\n");
         fflush(stdout);
         return nullptr;
      }

      ShowWindow(window, SW_SHOW);
      SetForegroundWindow(window);
      SetFocus(window);

      return window;
   }

   void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      switch (uMsg)
      {
      case WM_CLOSE:
         prepared = false;
         DestroyWindow(hWnd);
         PostQuitMessage(0);
         break;
      case WM_PAINT:
         ValidateRect(window, NULL);
         break;
      case WM_KEYDOWN:
         switch (wParam)
         {
         case KEY_P:
            paused = !paused;
            break;
         case KEY_F1:
            if (settings.overlay) {
               UIOverlay.visible = !UIOverlay.visible;
            }
            break;
         case KEY_ESCAPE:
            PostQuitMessage(0);
            break;
         }

         if (camera.type == Camera::firstperson)
         {
            switch (wParam)
            {
            case KEY_W:
               camera.keys.up = true;
               break;
            case KEY_S:
               camera.keys.down = true;
               break;
            case KEY_A:
               camera.keys.left = true;
               break;
            case KEY_D:
               camera.keys.right = true;
               break;
            }
         }

         keyPressed((uint32_t)wParam);
         break;
      case WM_KEYUP:
         if (camera.type == Camera::firstperson)
         {
            switch (wParam)
            {
            case KEY_W:
               camera.keys.up = false;
               break;
            case KEY_S:
               camera.keys.down = false;
               break;
            case KEY_A:
               camera.keys.left = false;
               break;
            case KEY_D:
               camera.keys.right = false;
               break;
            }
         }
         break;
      case WM_LBUTTONDOWN:
         mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
         mouseButtons.left = true;
         break;
      case WM_RBUTTONDOWN:
         mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
         mouseButtons.right = true;
         break;
      case WM_MBUTTONDOWN:
         mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
         mouseButtons.middle = true;
         break;
      case WM_LBUTTONUP:
         mouseButtons.left = false;
         break;
      case WM_RBUTTONUP:
         mouseButtons.right = false;
         break;
      case WM_MBUTTONUP:
         mouseButtons.middle = false;
         break;
      case WM_MOUSEWHEEL:
      {
         short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
         camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
         viewUpdated = true;
         break;
      }
      case WM_MOUSEMOVE:
      {
         handleMouseMove(LOWORD(lParam), HIWORD(lParam));
         break;
      }
      case WM_SIZE:
         if ((prepared) && (wParam != SIZE_MINIMIZED))
         {
            if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
            {
               destWidth = LOWORD(lParam);
               destHeight = HIWORD(lParam);
               windowResize();
            }
         }
         break;
      case WM_GETMINMAXINFO:
      {
         LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
         minMaxInfo->ptMinTrackSize.x = 64;
         minMaxInfo->ptMinTrackSize.y = 64;
         break;
      }
      case WM_ENTERSIZEMOVE:
         resizing = true;
         break;
      case WM_EXITSIZEMOVE:
         resizing = false;
         break;
      }
   }


   void VulkanExampleBase::viewChanged() {}

   void VulkanExampleBase::keyPressed(uint32_t) {}

   void VulkanExampleBase::mouseMoved(double x, double y, bool& handled) {}

   void VulkanExampleBase::buildCommandBuffers() {}

   void VulkanExampleBase::createSynchronizationPrimitives()
   {
      // Wait fences to sync command buffer access
      VkFenceCreateInfo fenceCreateInfo = VulkanInitializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
      waitFences.resize(drawCmdBuffers.size());
      for (auto& fence : waitFences) {
         VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
      }
   }

   void VulkanExampleBase::createCommandPool()
   {
      VkCommandPoolCreateInfo cmdPoolInfo = {};
      cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
      cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
   }

   void VulkanExampleBase::setupDepthStencil()
   {
      VkImageCreateInfo imageCI{};
      imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageCI.imageType = VK_IMAGE_TYPE_2D;
      imageCI.format = depthFormat;
      imageCI.extent = { width, height, 1 };
      imageCI.mipLevels = 1;
      imageCI.arrayLayers = 1;
      imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

      VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
      VkMemoryRequirements memReqs{};
      vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

      VkMemoryAllocateInfo memAllloc{};
      memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memAllloc.allocationSize = memReqs.size;
      memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
      VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

      VkImageViewCreateInfo imageViewCI{};
      imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewCI.image = depthStencil.image;
      imageViewCI.format = depthFormat;
      imageViewCI.subresourceRange.baseMipLevel = 0;
      imageViewCI.subresourceRange.levelCount = 1;
      imageViewCI.subresourceRange.baseArrayLayer = 0;
      imageViewCI.subresourceRange.layerCount = 1;
      imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
      if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
         imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
   }

   void VulkanExampleBase::setupFrameBuffer()
   {
      VkImageView attachments[2];

      // Depth/Stencil attachment is the same for all frame buffers
      attachments[1] = depthStencil.view;

      VkFramebufferCreateInfo frameBufferCreateInfo = {};
      frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      frameBufferCreateInfo.pNext = NULL;
      frameBufferCreateInfo.renderPass = _renderPass->vulkanRenderPass();
      frameBufferCreateInfo.attachmentCount = 2;
      frameBufferCreateInfo.pAttachments = attachments;
      frameBufferCreateInfo.width = width;
      frameBufferCreateInfo.height = height;
      frameBufferCreateInfo.layers = 1;

      // Create frame buffers for every swap chain image
      frameBuffers.resize(swapChain.imageCount);
      for (uint32_t i = 0; i < frameBuffers.size(); i++)
      {
         attachments[0] = swapChain.buffers[i].view;
         VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
      }
   }

   void VulkanExampleBase::setupRenderPass()
   {
      if (!_device)
      {
         _device = new genesis::Device(_physicalDevice, VulkanExampleBase::device, VulkanExampleBase::queue, VulkanExampleBase::cmdPool);
      }
      _renderPass = new genesis::RenderPass(_device, swapChain.colorFormat, depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR);
   }

   void VulkanExampleBase::getEnabledFeatures() {}

   void VulkanExampleBase::windowResize()
   {
      if (!prepared)
      {
         return;
      }
      prepared = false;
      resized = true;

      // Ensure all operations on the device have been finished before destroying resources
      vkDeviceWaitIdle(device);

      // Recreate swap chain
      width = destWidth;
      height = destHeight;
      setupSwapChain();

      // Recreate the frame buffers
      vkDestroyImageView(device, depthStencil.view, nullptr);
      vkDestroyImage(device, depthStencil.image, nullptr);
      vkFreeMemory(device, depthStencil.mem, nullptr);
      setupDepthStencil();
      for (uint32_t i = 0; i < frameBuffers.size(); i++) {
         vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
      }
      setupFrameBuffer();

      if ((width > 0.0f) && (height > 0.0f)) {
         if (settings.overlay) {
            UIOverlay.resize(width, height);
         }
      }

      // Command buffers need to be recreated as they may store
      // references to the recreated frame buffer
      destroyCommandBuffers();
      createCommandBuffers();
      buildCommandBuffers();

      vkDeviceWaitIdle(device);

      if ((width > 0.0f) && (height > 0.0f)) {
         camera.updateAspectRatio((float)width / (float)height);
      }

      // Notify derived class
      windowResized();
      viewChanged();

      prepared = true;
   }

   void VulkanExampleBase::handleMouseMove(int32_t x, int32_t y)
   {
      int32_t dx = (int32_t)mousePos.x - x;
      int32_t dy = (int32_t)mousePos.y - y;

      bool handled = false;

      if (settings.overlay) {
         ImGuiIO& io = ImGui::GetIO();
         handled = io.WantCaptureMouse;
      }
      mouseMoved((float)x, (float)y, handled);

      if (handled) {
         mousePos = glm::vec2((float)x, (float)y);
         return;
      }

      if (mouseButtons.left) {
         camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
         viewUpdated = true;
      }
      if (mouseButtons.right) {
         camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
         viewUpdated = true;
      }
      if (mouseButtons.middle) {
         camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
         viewUpdated = true;
      }
      mousePos = glm::vec2((float)x, (float)y);
   }

   void VulkanExampleBase::windowResized() {}

   void VulkanExampleBase::initSwapchain()
   {
#if defined(_WIN32)
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
      swapChain.initSurface(width, height);
#endif
   }

   void VulkanExampleBase::setupSwapChain()
   {
      swapChain.create(&width, &height, settings.vsync);
   }

   void VulkanExampleBase::OnUpdateUIOverlay(vks::UIOverlay* overlay) {}


}