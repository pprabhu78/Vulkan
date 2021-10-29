#include "Shader.h"
#include "Device.h"
#include "GenAssert.h"

#include <fstream>
#include <vector>
#include <iostream>

namespace genesis
{
   Shader::Shader(Device* device)
      : _device(device)
      , _valid(false)
      , _shaderStageInfo()
      , _shaderModuleInfo()
   {
      // nothing else
   }

   Shader::~Shader(void)
   {
      if (_shaderStageInfo.module != 0)
      {
         vkDestroyShaderModule(_device->vulkanDevice(), _shaderStageInfo.module, nullptr);
      }
   }

   void Shader::loadFromFile(const std::string& fileName, ShaderType shaderType)
   {
      _valid = false;

      std::ifstream ifs(fileName, std::ios::binary);
      std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(ifs), {});

      _shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      _shaderModuleInfo.pNext = nullptr;
      _shaderModuleInfo.flags = 0;
      _shaderModuleInfo.pCode = (uint32_t*)buffer.data();
      _shaderModuleInfo.codeSize = buffer.size();

      VkShaderModule shaderModule = 0;
      VkResult vkResult = vkCreateShaderModule(_device->vulkanDevice(), &_shaderModuleInfo, nullptr, &shaderModule);
      if (vkResult != VK_SUCCESS)
      {
         std::cout << "failed vkCreateShaderModule" << std::endl;
      }

      GEN_ASSERT(shaderModule != VK_NULL_HANDLE);

      _shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      _shaderStageInfo.pNext = nullptr;
      _shaderStageInfo.flags = 0;

      switch (shaderType)
      {
      case genesis::ST_VERTEX_SHADER:
         _shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
         break;
      case genesis::ST_FRAGMENT_SHADER:
         _shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
         break;
      default:
         break;
      }

      _shaderStageInfo.module = shaderModule;
      _shaderStageInfo.pName = "main";

      _valid = true;
   }

   bool Shader::valid(void) const
   {
      return _valid;
   }

   VkShaderModuleCreateInfo Shader::shaderModuleInfo(void) const
   {
      return _shaderModuleInfo;
   }
   VkPipelineShaderStageCreateInfo Shader::shaderStageInfo(void) const
   {
      return _shaderStageInfo;
   }
}