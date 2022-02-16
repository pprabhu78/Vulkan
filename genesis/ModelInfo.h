#pragma once

#include <string>

namespace genesis
{
   class Device;
   class VulkanGltfModel;

   class ModelInfo
   {
   public:
      ModelInfo(Device* device, const std::string& modelFileName, int modelId, int modelLoadingFlags);
      virtual ~ModelInfo();

   public:
      int modelId(void) const;
      const VulkanGltfModel* model(void) const;
   protected:
      Device* _device;
      VulkanGltfModel* _model;
      const int _modelId;
   };
}