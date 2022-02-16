#include "ModelInfo.h"
#include "VulkanGltf.h"

namespace genesis
{
   ModelInfo::ModelInfo(Device* device, const std::string& modelFileName, int modelId, int modelLoadingFlags)
      : _device(device)
      , _modelId(modelId)
   {
      _model = new VulkanGltfModel(_device, modelLoadingFlags);
      _model->loadFromFile(modelFileName, modelLoadingFlags);
   }

   ModelInfo::~ModelInfo()
   {
      delete _model;
   }

   int ModelInfo::modelId(void) const
   {
      return _modelId;
   }

   const VulkanGltfModel* ModelInfo::model(void) const
   {
      return _model;
   }
}

