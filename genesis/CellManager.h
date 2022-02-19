#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <atomic>

namespace genesis
{
   class Device;

   class ModelRegistry;

   class Cell;

   class CellManager
   {
   public:
      CellManager(Device* device, int modelLoadingFlags);
      virtual ~CellManager();
   public:
      virtual void addInstance(const std::string& modelFileName, const glm::mat4& xform);

      virtual void buildTlases(void);
      virtual void buildLayouts(void);

      virtual const Cell* cell(int cellIndex) const;

      virtual void buildDrawBuffers(void);
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const;
   protected:
      Device* _device;
      std::vector<Cell*> _cells;

      ModelRegistry* _modelRegistry;
   };
}
