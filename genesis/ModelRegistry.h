#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>

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

   class ModelRegistry
   {
   public:
      ModelRegistry(Device* device, int modelLoadingFlags);
      virtual ~ModelRegistry();

   public:
      virtual bool findModel(const std::string& modelName) const;
      virtual const ModelInfo* findModel(int modelId) const;

      virtual int modelId(const std::string& modelName) const;
      virtual void registerModel(const std::string& modelName);

   protected:
      Device* _device;

      std::atomic<int> _nextModelId;

      std::unordered_map<std::string, int> _mapModelNameToId;

      std::unordered_map<int, ModelInfo*> _mapModelIdToModelInfo;
      const int _modelLoadingFlags;
   };
}