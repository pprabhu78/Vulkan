#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <atomic>

namespace genesis
{
   class Device;
   class VulkanBuffer;

   enum AccelerationStructureType
   {
      BLAS = 0
    , TLAS
   };

   //! Ray tracing acceleration structure
   class AccelerationStructure
   {
   public:
      AccelerationStructure(Device* device, VkAccelerationStructureTypeKHR type, uint64_t sizeInBytes, const std::string& name = "");
      virtual ~AccelerationStructure();
   public:
      virtual const VkAccelerationStructureKHR& handle(void) const;
      virtual uint64_t deviceAddress(void) const;
   protected:
      VulkanBuffer* _buffer = nullptr;

      VkAccelerationStructureKHR _handle = 0;
      VkAccelerationStructureTypeKHR _type;

      Device* _device;

      static std::atomic<int> s_totalCount;
   };
}