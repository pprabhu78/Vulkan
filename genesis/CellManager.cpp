#include "CellManager.h"
#include "ModelRegistry.h"
#include "Cell.h"

#include <iostream>

namespace genesis
{
   CellManager::CellManager(Device* device, int modelLoadingFlags)
      : _device(device)
   {
      _modelRegistry = new ModelRegistry(_device, modelLoadingFlags);
   }

   CellManager::~CellManager()
   {
      for (auto cell : _cells)
      {
         delete cell;
      }
      delete _modelRegistry;
   }

   void CellManager::addInstance(const std::string& modelFileName, const glm::mat4& xform)
   {
      if (_cells.empty())
      {
         _cells.push_back(new Cell(_device, _modelRegistry));
      }
      Cell* cell = _cells[0];

      if (!_modelRegistry->findModel(modelFileName))
      {
         _modelRegistry->registerModel(modelFileName);
      }
      const int modelId = _modelRegistry->modelId(modelFileName);

      cell->addInstance(modelId, xform);
   }

   void CellManager::buildTlases()
   {
      for (Cell* cell : _cells)
      {
         cell->buildTlas();
      }
   }

   void CellManager::buildLayouts(void)
   {
      for (Cell* cell : _cells)
      {
         cell->buildLayout();
      }
   }

   const Cell* CellManager::cell(int cellIndex) const
   {
      if (cellIndex >= _cells.size())
      {
         std::cout << __FUNCTION__ << "warning: " << "cellIndex >= _cells.size()" << std::endl;
         return nullptr;
      }
      return _cells[cellIndex];
   }
}