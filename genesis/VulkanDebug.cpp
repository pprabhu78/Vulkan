#include "VulkanDebug.h"

namespace genesis
{
   std::string VulkanErrorToString::toString(VkResult result)
   {
      switch (result)
      {
#define STR(r) case VK_ ##r: return #r
         STR(NOT_READY);
         STR(TIMEOUT);
         STR(EVENT_SET);
         STR(EVENT_RESET);
         STR(INCOMPLETE);
         STR(ERROR_OUT_OF_HOST_MEMORY);
         STR(ERROR_OUT_OF_DEVICE_MEMORY);
         STR(ERROR_INITIALIZATION_FAILED);
         STR(ERROR_DEVICE_LOST);
         STR(ERROR_MEMORY_MAP_FAILED);
         STR(ERROR_LAYER_NOT_PRESENT);
         STR(ERROR_EXTENSION_NOT_PRESENT);
         STR(ERROR_FEATURE_NOT_PRESENT);
         STR(ERROR_INCOMPATIBLE_DRIVER);
         STR(ERROR_TOO_MANY_OBJECTS);
         STR(ERROR_FORMAT_NOT_SUPPORTED);
         STR(ERROR_SURFACE_LOST_KHR);
         STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
         STR(SUBOPTIMAL_KHR);
         STR(ERROR_OUT_OF_DATE_KHR);
         STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
         STR(ERROR_VALIDATION_FAILED_EXT);
         STR(ERROR_INVALID_SHADER_NV);
#undef STR
      default:
         return "UNKNOWN_ERROR";
      }
   }

   namespace debugmarker
   {
      bool active = false;

      PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
      PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
      PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
      PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
      PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

      void setup(VkDevice device)
      {
         pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
         pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
         pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
         pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
         pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

         // Set flag if at least one function pointer is present
         active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
      }

      void setObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (pfnDebugMarkerSetObjectName)
         {
            VkDebugMarkerObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.object = object;
            nameInfo.pObjectName = name;
            pfnDebugMarkerSetObjectName(device, &nameInfo);
         }
      }

      void setObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (pfnDebugMarkerSetObjectTag)
         {
            VkDebugMarkerObjectTagInfoEXT tagInfo = {};
            tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
            tagInfo.objectType = objectType;
            tagInfo.object = object;
            tagInfo.tagName = name;
            tagInfo.tagSize = tagSize;
            tagInfo.pTag = tag;
            pfnDebugMarkerSetObjectTag(device, &tagInfo);
         }
      }

      void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (pfnCmdDebugMarkerBegin)
         {
            VkDebugMarkerMarkerInfoEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
            memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
            markerInfo.pMarkerName = pMarkerName;
            pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
         }
      }

      void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (pfnCmdDebugMarkerInsert)
         {
            VkDebugMarkerMarkerInfoEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
            memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
            markerInfo.pMarkerName = markerName.c_str();
            pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
         }
      }

      void endRegion(VkCommandBuffer cmdBuffer)
      {
         // Check for valid function (may not be present if not running in a debugging application)
         if (pfnCmdDebugMarkerEnd)
         {
            pfnCmdDebugMarkerEnd(cmdBuffer);
         }
      }

      void setCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char* name)
      {
         setObjectName(device, (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name);
      }

      void setQueueName(VkDevice device, VkQueue queue, const char* name)
      {
         setObjectName(device, (uint64_t)queue, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name);
      }

      void setImageName(VkDevice device, VkImage image, const char* name)
      {
         setObjectName(device, (uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
      }

      void setSamplerName(VkDevice device, VkSampler sampler, const char* name)
      {
         setObjectName(device, (uint64_t)sampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name);
      }

      void setBufferName(VkDevice device, VkBuffer buffer, const char* name)
      {
         setObjectName(device, (uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
      }

      void setDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char* name)
      {
         setObjectName(device, (uint64_t)memory, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name);
      }

      void setShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char* name)
      {
         setObjectName(device, (uint64_t)shaderModule, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name);
      }

      void setPipelineName(VkDevice device, VkPipeline pipeline, const char* name)
      {
         setObjectName(device, (uint64_t)pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
      }

      void setPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char* name)
      {
         setObjectName(device, (uint64_t)pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name);
      }

      void setRenderPassName(VkDevice device, VkRenderPass renderPass, const char* name)
      {
         setObjectName(device, (uint64_t)renderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name);
      }

      void setFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char* name)
      {
         setObjectName(device, (uint64_t)framebuffer, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name);
      }

      void setDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const char* name)
      {
         setObjectName(device, (uint64_t)descriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name);
      }

      void setDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char* name)
      {
         setObjectName(device, (uint64_t)descriptorSet, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name);
      }

      void setSemaphoreName(VkDevice device, VkSemaphore semaphore, const char* name)
      {
         setObjectName(device, (uint64_t)semaphore, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name);
      }

      void setFenceName(VkDevice device, VkFence fence, const char* name)
      {
         setObjectName(device, (uint64_t)fence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name);
      }

      void setEventName(VkDevice device, VkEvent _event, const char* name)
      {
         setObjectName(device, (uint64_t)_event, VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name);
      }
   }

   namespace tools
   {
      void exitFatal(const std::string& message, int32_t exitCode)
      {
         std::cerr << message << "\n";
         exit(exitCode);
      }
      void exitFatal(const std::string& message, VkResult resultCode)
      {
         exitFatal(message, (int32_t)resultCode);
      }

      std::string errorString(VkResult errorCode)
      {
         switch (errorCode)
         {
#define STR(r) case VK_ ##r: return #r
            STR(NOT_READY);
            STR(TIMEOUT);
            STR(EVENT_SET);
            STR(EVENT_RESET);
            STR(INCOMPLETE);
            STR(ERROR_OUT_OF_HOST_MEMORY);
            STR(ERROR_OUT_OF_DEVICE_MEMORY);
            STR(ERROR_INITIALIZATION_FAILED);
            STR(ERROR_DEVICE_LOST);
            STR(ERROR_MEMORY_MAP_FAILED);
            STR(ERROR_LAYER_NOT_PRESENT);
            STR(ERROR_EXTENSION_NOT_PRESENT);
            STR(ERROR_FEATURE_NOT_PRESENT);
            STR(ERROR_INCOMPATIBLE_DRIVER);
            STR(ERROR_TOO_MANY_OBJECTS);
            STR(ERROR_FORMAT_NOT_SUPPORTED);
            STR(ERROR_SURFACE_LOST_KHR);
            STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR(SUBOPTIMAL_KHR);
            STR(ERROR_OUT_OF_DATE_KHR);
            STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR(ERROR_VALIDATION_FAILED_EXT);
            STR(ERROR_INVALID_SHADER_NV);
#undef STR
         default:
            return "UNKNOWN_ERROR";
         }
      }

      uint32_t alignedSize(uint32_t value, uint32_t alignment)
      {
         return (value + alignment - 1) & ~(alignment - 1);
      }
   }


}
