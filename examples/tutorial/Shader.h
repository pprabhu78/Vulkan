#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace genesis
{
   class Device;

   enum ShaderType
   {
        ST_VERTEX_SHADER = 0
      , ST_FRAGMENT_SHADER
   };

   class Shader
   {
   public:
      Shader(Device* device);
      virtual ~Shader(void);
   public:
      virtual void loadFromFile(const std::string& fileName, ShaderType shaderType);
      virtual bool valid(void) const;

      virtual VkShaderModuleCreateInfo shaderModuleInfo(void) const;
      virtual VkPipelineShaderStageCreateInfo shaderStageInfo(void) const;
   protected:
      VkShaderModuleCreateInfo _shaderModuleInfo;
      VkPipelineShaderStageCreateInfo _shaderStageInfo;

      Device* _device;

      bool _valid;
   };
}