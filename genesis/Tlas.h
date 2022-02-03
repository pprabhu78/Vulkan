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

   class Tlas
   {
   public:
      Tlas(Device* device);
      virtual ~Tlas();
   public:
      virtual void addInstance(const VulkanGltfModel* model, const glm::mat4& xform);

      virtual void build();

      virtual const VkAccelerationStructureKHR& handle(void) const;
   protected:
      std::unordered_map<const VulkanGltfModel*, Blas*> _mapModelToBlas;

      Device* _device = nullptr;

      AccelerationStructure* _tlas = nullptr;

      std::vector<VkAccelerationStructureInstanceKHR> _instances;
   };
}