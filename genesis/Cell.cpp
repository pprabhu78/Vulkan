#include "Cell.h"
#include "ModelRegistry.h"
#include "InstanceContainer.h"
#include "Tlas.h"
#include "IndirectLayout.h"

#include <iostream>

namespace genesis
{
   Cell::Cell(Device* device, ModelRegistry* modelRegistry)
      : _device(device)
      , _modelRegistry(modelRegistry)
   {
      _instanceContainer = new InstanceContainer(_device);
   }

   Cell::~Cell()
   {
      delete _indirectLayout;

      delete _tlas;

      delete _instanceContainer;
   }

   void Cell::addInstance(int modelId, const glm::mat4& xform)
   {
      _instanceContainer->addInstance(modelId, xform);
   }

   void Cell::buildTlas()
   {
      if (_tlas)
      {
         std::cout << "already built!!" << std::endl;
         return;
      }
      _tlas = new Tlas(_device, _modelRegistry);

      const auto& instances = _instanceContainer->instances();
      int instanceId = 0;
      for (const Instance& instance : instances)
      {
         _tlas->addInstance(instance);
      }

      _tlas->build();
   }

   const Tlas* Cell::tlas(void) const
   {
      return _tlas;
   }

   void Cell::buildLayout()
   {
      _indirectLayout = new IndirectLayout(_device, true);

      const ModelInfo* modelInfo = _modelRegistry->findModel(0);
      _indirectLayout->build(modelInfo->model());
   }

   const IndirectLayout* Cell::layout(void) const
   {
      return _indirectLayout;
   }
}