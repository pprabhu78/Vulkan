/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2019-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
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

   //! A swap chain is a collection of images used for rendering and presentation to the windowing system.
   //! These images are not created directly, but are created internally via vkCreateSwapchainKHR
   //! Flow of swap chain rendering:
   //! vkAcquireNextImageKHR(..., semaphoreToSignal, ...)
   //! vkQueueSubmit(..., semaphoreToSignal, semaphoreToWaitOn, ...)
   //! vkQueuePresentKHR(..., semaphoreToWaitOn, ...)
   //! -> 
   //! acquire signals presentComplete
   //! submit waits on presentComplete and signals renderComplete
   //! present waits on renderComplete
   class SwapChain
   {
   public:
      SwapChain(const Device* device);
      virtual ~SwapChain();

   public:

      //! platform specific initialization
#if defined(VK_USE_PLATFORM_WIN32_KHR)
   #if defined(VK_USE_PLATFORM_GLFW)
         void SwapChain::initSurface(GLFWwindow* window);
   #else
         void SwapChain::initSurface(void* platformHandle, void* platformWindow);
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
      //! create a swap chain
      virtual void create(uint32_t* width, uint32_t* height, bool vsync = false);

      //! clean up
      virtual void cleanup();

      //! get the next image to render to in the swap chain
      //! @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
      //! @param imageIndex Pointer to the image index that will be increased (set to) if the next image could be acquired
      //! * @note The function _will always wait_ until the next image has been acquired by internally setting timeout to UINT64_MAX
      virtual VkResult acquireNextImage(uint32_t& imageIndex, VkSemaphore presentCompleteSemaphore = VK_NULL_HANDLE);

      //! present the image
      //! @param queue Presentation queue for presenting the image
      //! @param imageIndex Index of the swapchain image to queue for presentation
      //! @param waitSemaphore(Optional) Semaphore that is waited on before the image is presented(only used if != VK_NULL_HANDLE)
      virtual VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

      //! what is the queue family index of this swap chain/surface
      virtual uint32_t presentationQueueFamilyIndex(void) const;

      //! what is the color format of this swap chain/surface
      virtual VkFormat colorFormat(void) const;

      //! what is the number of images of this swap chain/surface
      virtual uint32_t imageCount(void) const;

      //! The VkImage of the swap chain at a given index.
      virtual const VkImage& image(int index) const;

      //! The VkImageView of the swap chain image at a given index.
      virtual const VkImageView& imageView(int index) const;
   protected:
      //! init the queue family index that supports presentation for this swap chain/surface
      virtual void computePresentationQueueFamilyIndex();
      //! compute the color format and space for this swap chain/surface
      virtual void computeColorFormatAndSpace();
   protected:
      const Device* _device;

      VkSurfaceKHR _surface;
      uint32_t _presentationQueueFamilyIndex = UINT32_MAX;

      VkFormat _colorFormat = VK_FORMAT_UNDEFINED;
      VkColorSpaceKHR _colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

      VkSwapchainKHR _swapChain = VK_NULL_HANDLE;

      //! VkImage handles for the created swap chain
      //! These images are not created directly, their handles are gotten using vkGetSwapchainImagesKHR
      std::vector<VkImage> _images;
      //! These views are created based off of the _images above
      std::vector<VkImageView> _imageViews;
   };
}
