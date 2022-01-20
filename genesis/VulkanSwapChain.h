/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2017 by Sascha Willems - www.saschawillems.de
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
   #include "../../../../external/glfw/include/GLFW/glfw3.h"
   #include "../../../../external/glfw/include/GLFW/glfw3native.h"
#endif

#ifdef __ANDROID__
#include "VulkanAndroid.h"
#endif

namespace genesis
{
   typedef struct _SwapChainBuffers {
      VkImage image;
      VkImageView view;
   } SwapChainBuffer;

   class VulkanSwapChain
   {
   private:
      VkInstance instance;
      VkDevice device;
      VkPhysicalDevice physicalDevice;
      VkSurfaceKHR surface;
      // Function pointers
      PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
      PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
      PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
      PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
      PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
      PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
      PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
      PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
      PFN_vkQueuePresentKHR fpQueuePresentKHR;
   public:
      VkFormat colorFormat;
      VkColorSpaceKHR colorSpace;
      VkSwapchainKHR swapChain = VK_NULL_HANDLE;
      uint32_t imageCount;
      std::vector<VkImage> images;
      std::vector<SwapChainBuffer> buffers;
      uint32_t queueNodeIndex = UINT32_MAX;

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
      void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
      void create(uint32_t* width, uint32_t* height, bool vsync = false);
      VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
      VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
      void cleanup();
   };
}
