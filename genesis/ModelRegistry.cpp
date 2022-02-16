#include "ModelRegistry.h"
#include "VulkanGltf.h"
#include "ModelInfo.h"

#include <iostream>

namespace genesis
{
   ModelRegistry::ModelRegistry(Device* device, int modelLoadingFlags)
      : _device(device)
      , _modelLoadingFlags(modelLoadingFlags)
   {
      // nothing to do
   }

   ModelRegistry::~ModelRegistry()
   {
      for (auto& keyAndVal : _mapModelIdToModelInfo)
      {
         ModelInfo* modelInfo = keyAndVal.second;
         delete modelInfo;
      }
   }

   bool ModelRegistry::findModel(const std::string& modelName) const
   {
      auto it = _mapModelNameToId.find(modelName);
      return it != _mapModelNameToId.end();
   }

   const ModelInfo* ModelRegistry::findModel(int modelId) const
   {
      auto it = _mapModelIdToModelInfo.find(modelId);
      if (it == _mapModelIdToModelInfo.end())
      {
         std::cout << __FUNCTION__ << "warning: " << "it == _mapModelIdToName.end()" << std::endl;
         return nullptr;
      }
      return it->second;
   }

   int ModelRegistry::modelId(const std::string& modelName) const
   {
      auto it = _mapModelNameToId.find(modelName);
      if (it == _mapModelNameToId.end())
      {
         std::cout << __FUNCTION__ << "warning: " << "it == _mapModelNameToId.end()" << std::endl;
         return -1;
      }
      return it->second;
   }

   void ModelRegistry::registerModel(const std::string& modelFileName)
   {
      auto it = _mapModelNameToId.find(modelFileName);
      if (it != _mapModelNameToId.end())
      {
         std::cout << __FUNCTION__ << "warning: " << "it != _mapModelNameToId.end()" << std::endl;
         return;
      }

      const int modelId = _nextModelId++;

      ModelInfo* modelInfo = new ModelInfo(_device, modelFileName, modelId, _modelLoadingFlags);
      _mapModelIdToModelInfo.insert({ modelId, modelInfo });
      _mapModelNameToId.insert({ modelFileName, modelId });
   }

   int ModelRegistry::numModels(void) const
   {
      return (int)_mapModelIdToModelInfo.size();
   }
}