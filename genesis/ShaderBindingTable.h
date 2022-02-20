#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "Shader.h"

namespace genesis
{
   class Device;
   class VulkanBuffer;

   //! class to encapsulate the shader binding table
   //! Usage:
   //!  -add shaders of different types
   //!  -call build with the ray tracing pipeline
   class ShaderBindingTable
   {
   public:
      //! constructor
      ShaderBindingTable(Device* device);
      //! destructor
      virtual ~ShaderBindingTable();

   public:
      //! add a shader
      virtual void addShader(const std::string& shaderFileName, ShaderType shaderType);
      //! build using the ray tracing pipeline input
      virtual void build(VkPipeline raytracingPipeline);

      //! query the shader stages
      virtual const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages() const;

      //! query the shader groups
      virtual const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& shaderGroups() const;

      //! query the the 4 entry starts in the sbt
      virtual const VkStridedDeviceAddressRegionKHR& raygenEntry(void) const;
      virtual const VkStridedDeviceAddressRegionKHR& missEntry(void) const;
      virtual const VkStridedDeviceAddressRegionKHR& hitEntry(void) const;
      virtual const VkStridedDeviceAddressRegionKHR& callableEntry(void) const;

   protected:
      //! internal
      virtual Shader* loadShader(const std::string& fileName, genesis::ShaderType stage);
   protected:
      Device* _device;

      std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
      std::vector<Shader*> _shadersCreatedHere;
      std::vector<VkRayTracingShaderGroupCreateInfoKHR> _shaderGroups;

      VulkanBuffer* _raygenShaderBindingTable = nullptr;
      VulkanBuffer* _missShaderBindingTable = nullptr;
      VulkanBuffer* _hitShaderBindingTable = nullptr;

      VkStridedDeviceAddressRegionKHR _raygenShaderSbtEntry{};
      VkStridedDeviceAddressRegionKHR _missShaderSbtEntry{};
      VkStridedDeviceAddressRegionKHR _hitShaderSbtEntry{};
      VkStridedDeviceAddressRegionKHR _callableShaderSbtEntry{};
   };
}