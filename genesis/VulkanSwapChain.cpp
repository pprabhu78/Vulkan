/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2019-2022 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanSwapChain.h"
#include "VulkanDebug.h"
#include "VulkanFunctions.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "Instance.h"

namespace genesis
{
   VulkanSwapChain::VulkanSwapChain(const Device* device)
      : _device(device)
   {
      // no op
   }
   VulkanSwapChain::~VulkanSwapChain()
   {
      // no op
   }

   uint32_t VulkanSwapChain::presentationQueueFamilyIndex(void) const
   {
      return _presentationQueueFamilyIndex;
   }

   VkFormat VulkanSwapChain::colorFormat(void) const
   {
      return _colorFormat;
   }

   void VulkanSwapChain::computePresentationQueueFamilyIndex()
   {
      VkPhysicalDevice physicalDevice = _device->physicalDevice()->vulkanPhysicalDevice();
      const auto& queueProps = _device->physicalDevice()->queueFamilyProperties();

      // Iterate over each queue family to learn whether it supports presenting:
      // Find a queue with present support
      // Will be used to present the swap chain images to the windowing system
      std::vector<VkBool32> supportsPresent(queueProps.size());
      for (uint32_t i = 0; i < queueProps.size(); i++)
      {
         genesis::vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, _surface, &supportsPresent[i]);
      }

      // Search for a graphics and a present queue in the array of queue
      // families, try to find one that supports both
      uint32_t graphicsQueueNodeIndex = UINT32_MAX;
      uint32_t presentQueueNodeIndex = UINT32_MAX;
      for (uint32_t i = 0; i < queueProps.size(); i++)
      {
         if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
         {
            if (graphicsQueueNodeIndex == UINT32_MAX)
            {
               graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE)
            {
               graphicsQueueNodeIndex = i;
               presentQueueNodeIndex = i;
               break;
            }
         }
      }
      if (presentQueueNodeIndex == UINT32_MAX)
      {
         // If there's no queue that supports both present and graphics
         // try to find a separate present queue
         for (uint32_t i = 0; i < queueProps.size(); ++i)
         {
            if (supportsPresent[i] == VK_TRUE)
            {
               presentQueueNodeIndex = i;
               break;
            }
         }
      }

      // Exit if either a graphics or a presenting queue hasn't been found
      if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
      {
         tools::exitFatal("Could not find a graphics and/or presenting queue!", -1);
      }

      // todo : Add support for separate graphics and presenting queue
      if (graphicsQueueNodeIndex != presentQueueNodeIndex)
      {
         tools::exitFatal("Separate graphics and presenting queues are not supported yet!", -1);
      }

      _presentationQueueFamilyIndex = graphicsQueueNodeIndex;
   }

   void VulkanSwapChain::computeColorFormatAndSpace()
   {
      VkPhysicalDevice physicalDevice = _device->physicalDevice()->vulkanPhysicalDevice();

      // Get list of supported surface formats
      uint32_t formatCount;
      VK_CHECK_RESULT(genesis::vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, NULL));
      assert(formatCount > 0);

      std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
      VK_CHECK_RESULT(genesis::vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, surfaceFormats.data()));

      // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
      // there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
      if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
      {
         _colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
         _colorSpace = surfaceFormats[0].colorSpace;
      }
      else
      {
         // iterate over the list of available surface format and
         // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
         bool found_B8G8R8A8_UNORM = false;
         for (auto&& surfaceFormat : surfaceFormats)
         {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
               _colorFormat = surfaceFormat.format;
               _colorSpace = surfaceFormat.colorSpace;
               found_B8G8R8A8_UNORM = true;
               break;
            }
         }

         // in case VK_FORMAT_B8G8R8A8_UNORM is not available
         // select the first available color format
         if (!found_B8G8R8A8_UNORM)
         {
            _colorFormat = surfaceFormats[0].format;
            _colorSpace = surfaceFormats[0].colorSpace;
         }
      }
   }

   /** @brief Creates the platform specific surface abstraction of the native platform window used for presentation */
#if defined(VK_USE_PLATFORM_WIN32_KHR)
   #if defined(VK_USE_PLATFORM_GLFW)
      void VulkanSwapChain::initSurface(GLFWwindow* window)
   #else
      void VulkanSwapChain::initSurface(void* platformHandle, void* platformWindow)
   #endif
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
   void VulkanSwapChain::initSurface(ANativeWindow* window)
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
   void VulkanSwapChain::initSurface(IDirectFB* dfb, IDirectFBSurface* window)
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
   void VulkanSwapChain::initSurface(wl_display* display, wl_surface* window)
#elif defined(VK_USE_PLATFORM_XCB_KHR)
   void VulkanSwapChain::initSurface(xcb_connection_t* connection, xcb_window_t window)
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
   void VulkanSwapChain::initSurface(void* view)
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
   void VulkanSwapChain::initSurface(uint32_t width, uint32_t height)
#endif
   {
      VkResult err = VK_SUCCESS;

#if defined(VK_USE_PLATFORM_GLFW)
      _surface = 0;
      err = glfwCreateWindowSurface(_device->physicalDevice()->instance()->vulkanInstance(), window, nullptr, &_surface);
      if (err != VK_SUCCESS)
      {
         assert(!"Failed to create a Window surface");
      }
      // Create the os-specific surface
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
      VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = (HINSTANCE)platformHandle;
      surfaceCreateInfo.hwnd = (HWND)platformWindow;
      err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &_surface);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
      VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.window = window;
      err = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, NULL, &_surface);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
      VkIOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
      surfaceCreateInfo.pNext = NULL;
      surfaceCreateInfo.flags = 0;
      surfaceCreateInfo.pView = view;
      err = vkCreateIOSSurfaceMVK(instance, &surfaceCreateInfo, nullptr, &_surface);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
      VkMacOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
      surfaceCreateInfo.pNext = NULL;
      surfaceCreateInfo.flags = 0;
      surfaceCreateInfo.pView = view;
      err = vkCreateMacOSSurfaceMVK(instance, &surfaceCreateInfo, NULL, &_surface);
#elif defined(_DIRECT2DISPLAY)
      createDirect2DisplaySurface(width, height);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      VkDirectFBSurfaceCreateInfoEXT surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_DIRECTFB_SURFACE_CREATE_INFO_EXT;
      surfaceCreateInfo.dfb = dfb;
      surfaceCreateInfo.surface = window;
      err = vkCreateDirectFBSurfaceEXT(instance, &surfaceCreateInfo, nullptr, &_surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.display = display;
      surfaceCreateInfo.surface = window;
      err = vkCreateWaylandSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &_surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.connection = connection;
      surfaceCreateInfo.window = window;
      err = vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &_surface);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
      VkHeadlessSurfaceCreateInfoEXT surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
      PFN_vkCreateHeadlessSurfaceEXT fpCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT");
      if (!fpCreateHeadlessSurfaceEXT) {
         vks::tools::exitFatal("Could not fetch function pointer for the headless extension!", -1);
      }
      err = fpCreateHeadlessSurfaceEXT(instance, &surfaceCreateInfo, nullptr, &_surface);
#endif

      if (err != VK_SUCCESS) {
         tools::exitFatal("Could not create surface!", err);
      }

      // init the queue family index that supports presentation for this swap chain/surface
      computePresentationQueueFamilyIndex();

      // compute the color format and space for this swap chain/surface
      computeColorFormatAndSpace();
   }

   /**
   * Create the swapchain and get its images with given width and height
   *
   * @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
   * @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
   * @param vsync (Optional) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
   */
   void VulkanSwapChain::create(uint32_t* width, uint32_t* height, bool vsync)
   {
      VkDevice device = _device->vulkanDevice();
      VkPhysicalDevice physicalDevice = _device->physicalDevice()->vulkanPhysicalDevice();

      // Store the current swap chain handle so we can use it later on to ease up recreation
      VkSwapchainKHR oldSwapchain = _swapChain;

      // Get physical device surface properties and formats
      VkSurfaceCapabilitiesKHR surfCaps;
      VK_CHECK_RESULT(genesis::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, _surface, &surfCaps));

      // Get available present modes
      uint32_t presentModeCount;
      VK_CHECK_RESULT(genesis::vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentModeCount, NULL));
      assert(presentModeCount > 0);

      std::vector<VkPresentModeKHR> presentModes(presentModeCount);
      VK_CHECK_RESULT(genesis::vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentModeCount, presentModes.data()));

      VkExtent2D swapchainExtent = {};
      // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
      if (surfCaps.currentExtent.width == (uint32_t)-1)
      {
         // If the surface size is undefined, the size is set to
         // the size of the images requested.
         swapchainExtent.width = *width;
         swapchainExtent.height = *height;
      }
      else
      {
         // If the surface size is defined, the swap chain size must match
         swapchainExtent = surfCaps.currentExtent;
         *width = surfCaps.currentExtent.width;
         *height = surfCaps.currentExtent.height;
      }


      // Select a present mode for the swapchain

      // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
      // This mode waits for the vertical blank ("v-sync")
      VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

      // If v-sync is not requested, try to find a mailbox mode
      // It's the lowest latency non-tearing present mode available
      if (!vsync)
      {
         for (size_t i = 0; i < presentModeCount; i++)
         {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
               swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
               break;
            }
            if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
               swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
         }
      }

      // Determine the number of images
      uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
      if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
      {
         desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
      }

      // Find the transformation of the surface
      VkSurfaceTransformFlagsKHR preTransform;
      if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      {
         // We prefer a non-rotated transform
         preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
      }
      else
      {
         preTransform = surfCaps.currentTransform;
      }

      // Find a supported composite alpha format (not all devices support alpha opaque)
      VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      // Simply select the first composite alpha format available
      std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
         VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
         VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
         VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
         VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      };
      for (auto& compositeAlphaFlag : compositeAlphaFlags) {
         if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
         };
      }

      VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
      swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      swapChainCreateInfo.surface = _surface;
      swapChainCreateInfo.minImageCount = desiredNumberOfSwapchainImages;
      swapChainCreateInfo.imageFormat = _colorFormat;
      swapChainCreateInfo.imageColorSpace = _colorSpace;
      swapChainCreateInfo.imageExtent = { swapchainExtent.width, swapchainExtent.height };
      swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      swapChainCreateInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
      swapChainCreateInfo.imageArrayLayers = 1;
      swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapChainCreateInfo.queueFamilyIndexCount = 0;
      swapChainCreateInfo.presentMode = swapchainPresentMode;
      // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
      swapChainCreateInfo.oldSwapchain = oldSwapchain;
      // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
      swapChainCreateInfo.clipped = VK_TRUE;
      swapChainCreateInfo.compositeAlpha = compositeAlpha;

      // Enable transfer source on swap chain images if supported
      if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
         swapChainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      }

      // Enable transfer destination on swap chain images if supported
      if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
         swapChainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      }

      VK_CHECK_RESULT(genesis::vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapChain));

      // If an existing swap chain is re-created, destroy the old swap chain
      // This also cleans up all the presentable images
      if (oldSwapchain != VK_NULL_HANDLE)
      {
         for (uint32_t i = 0; i < _imageCount; i++)
         {
            vkDestroyImageView(device, _buffers[i].view, nullptr);
         }
         genesis::vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
      }
      VK_CHECK_RESULT(genesis::vkGetSwapchainImagesKHR(device, _swapChain, &_imageCount, NULL));

      // Get the swap chain images
      _images.resize(_imageCount);
      VK_CHECK_RESULT(genesis::vkGetSwapchainImagesKHR(device, _swapChain, &_imageCount, _images.data()));

      // Get the swap chain buffers containing the image and imageview
      _buffers.resize(_imageCount);
      for (uint32_t i = 0; i < _imageCount; i++)
      {
         VkImageViewCreateInfo colorAttachmentView = {};
         colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
         colorAttachmentView.pNext = NULL;
         colorAttachmentView.format = _colorFormat;
         colorAttachmentView.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
         };
         colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         colorAttachmentView.subresourceRange.baseMipLevel = 0;
         colorAttachmentView.subresourceRange.levelCount = 1;
         colorAttachmentView.subresourceRange.baseArrayLayer = 0;
         colorAttachmentView.subresourceRange.layerCount = 1;
         colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
         colorAttachmentView.flags = 0;

         _buffers[i].image = _images[i];

         colorAttachmentView.image = _buffers[i].image;

         VK_CHECK_RESULT(vkCreateImageView(device, &colorAttachmentView, nullptr, &_buffers[i].view));
      }
   }

   /**
   * Acquires the next image in the swap chain
   *
   * @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
   * @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
   *
   * @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
   *
   * @return VkResult of the image acquisition
   */
   VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
   {
      // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
      // With that we don't have to handle VK_NOT_READY
      return genesis::vkAcquireNextImageKHR(_device->vulkanDevice(), _swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
   }

   /**
   * Queue an image for presentation
   *
   * @param queue Presentation queue for presenting the image
   * @param imageIndex Index of the swapchain image to queue for presentation
   * @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
   *
   * @return VkResult of the queue presentation
   */
   VkResult VulkanSwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
   {
      VkPresentInfoKHR presentInfo = {};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.pNext = NULL;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &_swapChain;
      presentInfo.pImageIndices = &imageIndex;
      // Check if a wait semaphore has been specified to wait for before presenting the image
      if (waitSemaphore != VK_NULL_HANDLE)
      {
         presentInfo.pWaitSemaphores = &waitSemaphore;
         presentInfo.waitSemaphoreCount = 1;
      }
      return genesis::vkQueuePresentKHR(queue, &presentInfo);
   }


   /**
   * Destroy and free Vulkan resources used for the swapchain
   */
   void VulkanSwapChain::cleanup()
   {
      if (_swapChain != VK_NULL_HANDLE)
      {
         for (uint32_t i = 0; i < _imageCount; i++)
         {
            vkDestroyImageView(_device->vulkanDevice(), _buffers[i].view, nullptr);
         }
      }
      if (_surface != VK_NULL_HANDLE)
      {
         genesis::vkDestroySwapchainKHR(_device->vulkanDevice(), _swapChain, nullptr);
         vkDestroySurfaceKHR(_device->physicalDevice()->instance()->vulkanInstance(), _surface, nullptr);
      }
      _surface = VK_NULL_HANDLE;
      _swapChain = VK_NULL_HANDLE;
   }
}


