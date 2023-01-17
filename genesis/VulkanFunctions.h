#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{  
   extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
   extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
   extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
   extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
   extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
   extern PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
   extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
   extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
   extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
   extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

   extern PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;

   class Device;

   class VulkanFunctionsInitializer
   {
   public:
      static void initialize(Device* device);
   protected:
      static bool s_initialized;
   };
}