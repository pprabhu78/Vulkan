#include "InstanceContainer.h"

namespace genesis
{
   InstanceContainer::InstanceContainer(Device* device)
      : _device(device)
      , _nextInstanceId(0)
   {
      // nothing else to do
   }

   InstanceContainer::~InstanceContainer()
   {
      // nothing else to do
   }

   uint32_t InstanceContainer::addInstance(int modelId, const glm::mat4& xform)
   {
      const uint32_t instanceId = _nextInstanceId++;

      Instance instance;
      instance._modelId = modelId;
      instance._xform = xform;

      _instances.push_back(instance);

      auto& setInstances = _mapModelIdToInstanceIds[modelId];
      setInstances.insert(instanceId);

      return instanceId;
   }

   const std::vector<Instance>& InstanceContainer::instances(void) const
   {
      return _instances;
   }
}