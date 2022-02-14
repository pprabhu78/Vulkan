#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include <unordered_map>

namespace genesis
{
   class Device;
   class VulkanGltfModel;
   class Blas;
   class AccelerationStructure;
   class VulkanBuffer;
   class ModelRegistry;

   class Instance;

   class Tlas
   {
   public:
      Tlas(Device* device, const ModelRegistry* modelRegistry);
      virtual ~Tlas();
   public:
      virtual void addInstance(const Instance& instance);

      virtual void build();

      virtual const VkAccelerationStructureKHR& handle(void) const;
   protected:

      Device* _device = nullptr;

      AccelerationStructure* _tlas = nullptr;

      std::vector<VkAccelerationStructureInstanceKHR> _vulkanInstances;

      const ModelRegistry* _modelRegistry;

      std::unordered_map<int, Blas*> _mapModelToBlas;
   };
}