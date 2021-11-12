#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Device;

   enum AccelerationStructureType
   {
      BLAS = 0
    , TLAS
   };

   //! Ray tracing acceleration structure
   class AccelerationStructure
   {
   public:
      AccelerationStructure(Device* device, AccelerationStructureType type, uint64_t sizeInBytes);
      virtual ~AccelerationStructure();
   public:
      virtual const VkAccelerationStructureKHR& handle(void) const;
      virtual uint64_t deviceAddress(void) const;
   protected:
      VkAccelerationStructureKHR _handle = 0;
      uint64_t _deviceAddress = 0;
      VkDeviceMemory _memory = 0;
      VkBuffer _buffer = 0;

      Device* _device;
      AccelerationStructureType _type;
   };
}