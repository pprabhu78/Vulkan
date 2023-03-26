/*
* Class wrapping access to the Vulkan extension functions (KHR, EXT, etc)
*
* Copyright (C) 2019-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{  
   class Device;

   class vkExtensions
   {
   public:
      vkExtensions();
      virtual ~vkExtensions();
   public:
      virtual void initialize(Device* device);
   public:
      PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
      PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
      PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
      PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
      PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
      PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR = nullptr;
      PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
      PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
      PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
      PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

      // swap chain functions
      PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
      PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
      PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
      PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;

      PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
      PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
      PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
      PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
      PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

      // mesh shaders
      PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;

      // dynamic rendering
      PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR = nullptr;
      PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR = nullptr;

      // inter-op functionality
      PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR = nullptr;
      PFN_vkGetSemaphoreWin32HandleKHR    vkGetSemaphoreWin32HandleKHR = nullptr;
      PFN_vkGetMemoryWin32HandleKHR    vkGetMemoryWin32HandleKHR = nullptr;
   protected:
      bool _initialized = false;
   };
}