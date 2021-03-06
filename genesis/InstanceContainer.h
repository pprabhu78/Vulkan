#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace genesis
{
   class Device;

   class Instance
   {
   public:
      glm::mat4 _xform;
      int _instanceId;
      int _modelId;
      int pad0;
      int pad1;
   };

   class InstanceContainer
   {
   public:
      InstanceContainer(Device* device);
      virtual ~InstanceContainer();
   public:
      virtual uint32_t addInstance(int modelId, const glm::mat4& xform);

      virtual const std::vector<Instance>& instances(void) const;

      virtual const std::unordered_map<uint32_t, std::unordered_set<uint32_t> >& mapModelIdsToInstances(void) const;
   protected:
      Device* _device;
      std::atomic<uint32_t> _nextInstanceId;

      std::vector<Instance> _instances;

      //! map of model id to the instance ids with this model id
      std::unordered_map<uint32_t, std::unordered_set<uint32_t> > _mapModelIdToInstanceIds;
   };
}