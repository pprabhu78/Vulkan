#pragma once

#include "GenMath.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace genesis
{
   class Device;

   class ModelRegistry;
   class InstanceContainer;

   class Tlas;
   class IndirectLayout;

   //! A cell is a world cell. It contains
   //! a bunch of instances of different models
   //! It is managed by the cell manager
   class Cell
   {
   public:
      //! in this case, registry is externally provided
      Cell(Device* device, ModelRegistry* modelRegistry);

      virtual ~Cell();
   private:
      //! not allowed
      Cell() = delete;
      Cell(const Cell& rhs) = delete;
      Cell& operator=(const Cell& rhs) = delete;
      Cell(Device* device) = delete;
   public:
      virtual void addInstance(int modelId, const glm::mat4& xform);

      virtual void buildTlas(void);
      virtual const Tlas* tlas(void) const;

      virtual void buildLayout(void);
      virtual const IndirectLayout* layout(void) const;

      virtual void buildDrawBuffer(void);
      virtual void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const;
   protected:
      Device* _device;

      InstanceContainer* _instanceContainer = nullptr;

      Tlas* _tlas = nullptr;

      const ModelRegistry* _modelRegistry = nullptr;

      IndirectLayout* _indirectLayout = nullptr;
   };
}