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

      PFN_vkSetDebugUtilsObjectNameEXT       vkSetDebugUtilsObjectNameEXT          = VK_NULL_HANDLE;
      PFN_vkSetDebugUtilsObjectTagEXT        vkSetDebugUtilsObjectTagEXT           = VK_NULL_HANDLE;
      PFN_vkCmdBeginDebugUtilsLabelEXT     vkCmdBeginDebugUtilsLabelEXT = VK_NULL_HANDLE;
      PFN_vkCmdEndDebugUtilsLabelEXT       vkCmdEndDebugUtilsLabelEXT = VK_NULL_HANDLE;
      PFN_vkCmdInsertDebugUtilsLabelEXT    vkCmdInsertDebugUtilsLabelEXT = VK_NULL_HANDLE;
     
      void setup(VkDevice device)
      {
         pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
         pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
         pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
         pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
         pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

         vkSetDebugUtilsObjectNameEXT     = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
         vkSetDebugUtilsObjectTagEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectTagEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectTagEXT"));
         vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
         vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
         vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));

         // Set flag if at least one function pointer is present
         active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
      }

      void setObjectName(VkDevice device, uint64_t object, VkObjectType objectType, const char* name)
      {
         if (vkSetDebugUtilsObjectNameEXT)
         {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = object;
            nameInfo.pObjectName = name;
            vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
         }
      }

      void setObjectTag(VkDevice device, uint64_t object, VkObjectType objectType, uint64_t name, size_t tagSize, const void* tag)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (vkSetDebugUtilsObjectNameEXT)
         {
            VkDebugUtilsObjectTagInfoEXT tagInfo = {};
            tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            tagInfo.objectType = objectType;
            tagInfo.objectHandle = object;
            tagInfo.tagName = name;
            tagInfo.tagSize = tagSize;
            tagInfo.pTag = tag;
            vkSetDebugUtilsObjectTagEXT(device, &tagInfo);
         }
      }

      void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (vkCmdBeginDebugUtilsLabelEXT)
         {
            VkDebugUtilsLabelEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
            markerInfo.pLabelName = pMarkerName;
            vkCmdBeginDebugUtilsLabelEXT(cmdbuffer, &markerInfo);
         }
      }

      void endRegion(VkCommandBuffer cmdBuffer)
      {
         // Check for valid function (may not be present if not running in a debugging application)
         if (vkCmdEndDebugUtilsLabelEXT)
         {
            vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
         }
      }

      void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
      {
         // Check for valid function pointer (may not be present if not running in a debugging application)
         if (vkCmdInsertDebugUtilsLabelEXT)
         {
            VkDebugUtilsLabelEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
            markerInfo.pLabelName = markerName.c_str();
            vkCmdInsertDebugUtilsLabelEXT(cmdbuffer, &markerInfo);
         }
      }

      void setCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char* name)
      {
         setObjectName(device, (uint64_t)cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, name);
      }

      void setQueueName(VkDevice device, VkQueue queue, const char* name)
      {
         setObjectName(device, (uint64_t)queue, VK_OBJECT_TYPE_QUEUE, name);
      }

      void setImageName(VkDevice device, VkImage image, const char* name)
      {
         setObjectName(device, (uint64_t)image, VK_OBJECT_TYPE_IMAGE, name);
      }

      void setSamplerName(VkDevice device, VkSampler sampler, const char* name)
      {
         setObjectName(device, (uint64_t)sampler, VK_OBJECT_TYPE_SAMPLER, name);
      }

      void setBufferName(VkDevice device, VkBuffer buffer, const char* name)
      {
         setObjectName(device, (uint64_t)buffer, VK_OBJECT_TYPE_BUFFER, name);
      }

      void setDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char* name)
      {
         setObjectName(device, (uint64_t)memory, VK_OBJECT_TYPE_DEVICE_MEMORY, name);
      }

      void setShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char* name)
      {
         setObjectName(device, (uint64_t)shaderModule, VK_OBJECT_TYPE_SHADER_MODULE, name);
      }

      void setPipelineName(VkDevice device, VkPipeline pipeline, const char* name)
      {
         setObjectName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, name);
      }

      void setPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char* name)
      {
         setObjectName(device, (uint64_t)pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name);
      }

      void setRenderPassName(VkDevice device, VkRenderPass renderPass, const char* name)
      {
         setObjectName(device, (uint64_t)renderPass, VK_OBJECT_TYPE_RENDER_PASS, name);
      }

      void setFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char* name)
      {
         setObjectName(device, (uint64_t)framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, name);
      }

      void setDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const char* name)
      {
         setObjectName(device, (uint64_t)descriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, name);
      }

      void setDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char* name)
      {
         setObjectName(device, (uint64_t)descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, name);
      }

      void setSemaphoreName(VkDevice device, VkSemaphore semaphore, const char* name)
      {
         setObjectName(device, (uint64_t)semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
      }

      void setFenceName(VkDevice device, VkFence fence, const char* name)
      {
         setObjectName(device, (uint64_t)fence, VK_OBJECT_TYPE_FENCE, name);
      }

      void setEventName(VkDevice device, VkEvent _event, const char* name)
      {
         setObjectName(device, (uint64_t)_event, VK_OBJECT_TYPE_EVENT, name);
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
