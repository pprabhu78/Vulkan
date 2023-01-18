/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <vulkan/vulkan.h>

#if VK_USE_PLATFORM_GLFW
   #ifdef _WIN32
      #define GLFW_EXPOSE_NATIVE_WIN32
   #endif
   #include "../external/glfw/include/GLFW/glfw3.h"
   #include "../external/glfw/include/GLFW/glfw3native.h"
#endif

#ifdef __ANDROID__
#include "VulkanAndroid.h"
#endif

namespace genesis
{
   class Device;

   typedef struct _SwapChainBuffers {
      VkImage image;
      VkImageView view;
   } SwapChainBuffer;

   class VulkanSwapChain
   {
   public:
      VulkanSwapChain(const Device* device);
      virtual ~VulkanSwapChain();

   public:

      //! platform specific initialization
#if defined(VK_USE_PLATFORM_WIN32_KHR)
   #if defined(VK_USE_PLATFORM_GLFW)
         void VulkanSwapChain::initSurface(GLFWwindow* window);
   #else
         void VulkanSwapChain::initSurface(void* platformHandle, void* platformWindow);
   #endif
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
      void initSurface(ANativeWindow* window);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      void initSurface(IDirectFB* dfb, IDirectFBSurface* window);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      void initSurface(wl_display* display, wl_surface* window);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      void initSurface(xcb_connection_t* connection, xcb_window_t window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
      void initSurface(void* view);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
      void initSurface(uint32_t width, uint32_t height);
#if defined(_DIRECT2DISPLAY)
      void createDirect2DisplaySurface(uint32_t width, uint32_t height);
#endif
#endif

      virtual void create(uint32_t* width, uint32_t* height, bool vsync = false);
      virtual VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
      virtual VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
      virtual void cleanup();

      virtual uint32_t presentationQueueFamilyIndex(void) const;
      virtual VkFormat colorFormat(void) const;
   public:

      VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
      uint32_t _imageCount;
      std::vector<VkImage> _images;
      std::vector<SwapChainBuffer> _buffers;

   protected:
      //! init the queue family index that supports presentation for this swap chain/surface
      virtual void computePresentationQueueFamilyIndex();
      //! compute the color format and space for this swap chain/surface
      virtual void computeColorFormatAndSpace();
   protected:
      const Device* _device;

      VkSurfaceKHR _surface;
      uint32_t _presentationQueueFamilyIndex = UINT32_MAX;

      VkFormat _colorFormat;
      VkColorSpaceKHR _colorSpace;
   };
}
