#pragma once

#include <vulkan/vulkan.h>

namespace genesis
{
   class Device;
   class VulkanGltfModel;
   class AccelerationStructure;

   class Blas
   {
   public:
      //! construct from a given model
      Blas(Device* device, VulkanGltfModel* model);

      //! destructor
      virtual ~Blas();
   public:
      virtual uint64_t deviceAddress(void) const;
   protected:
      virtual void build();
   protected:
      Device* _device = nullptr;
      VulkanGltfModel* _model = nullptr;
      AccelerationStructure* _blas = nullptr;
   };
}