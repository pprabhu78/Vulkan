#include "ShaderBindingTable.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "VkExtensions.h"
#include "VulkanDebug.h"
#include "Buffer.h"

namespace genesis
{
   ShaderBindingTable::ShaderBindingTable(Device* device)
      : _device(device)
   {
      // nothing else to do
   }

   ShaderBindingTable::~ShaderBindingTable()
   {
      for (auto shader : _shadersCreatedHere)
      {
         delete shader;
      }

      delete _raygenShaderBindingTable;
      delete _missShaderBindingTable;
      delete _hitShaderBindingTable;
   }

   Shader* ShaderBindingTable::loadShader(const std::string& fileName, genesis::ShaderType stage)
   {
      genesis::Shader* shader = new genesis::Shader(_device);
      shader->loadFromFile(fileName, stage);

      return shader;
   }

   void ShaderBindingTable::addShader(const std::string& shaderFileName, ShaderType shaderType)
   {
      Shader* shader = loadShader(shaderFileName, shaderType);
      _shaderStages.push_back(shader->pipelineShaderStageCreateInfo());
      _shadersCreatedHere.push_back(shader);

      VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
      shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;

      if (shaderType == ST_RT_RAYGEN || shaderType == ST_RT_MISS)
      {
         shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
         shaderGroup.generalShader = static_cast<uint32_t>(_shaderStages.size()) - 1;
         shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
         shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
         shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
      }
      else if (shaderType == ST_RT_CLOSEST_HIT)
      {
         shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
         shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
         shaderGroup.closestHitShader = static_cast<uint32_t>(_shaderStages.size()) - 1;
         shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
         shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
      }
      _shaderGroups.push_back(shaderGroup);
   }

   static uint32_t alignedSize(uint32_t value, uint32_t alignment)
   {
      return (value + alignment - 1) & ~(alignment - 1);
   }

   void ShaderBindingTable::build(VkPipeline raytracingPipeline)
   {
      const uint32_t handleSize = _device->physicalDevice()->rayTracingPipelineProperties().shaderGroupHandleSize;
      const uint32_t handleSizeAligned = alignedSize(_device->physicalDevice()->rayTracingPipelineProperties().shaderGroupHandleSize, _device->physicalDevice()->rayTracingPipelineProperties().shaderGroupHandleAlignment);
      const uint32_t groupCount = static_cast<uint32_t>(_shaderGroups.size());
      const uint32_t sbtSize = groupCount * handleSizeAligned;

      std::vector<uint8_t> shaderHandleStorage(sbtSize);
      VK_CHECK_RESULT(_device->extensions().vkGetRayTracingShaderGroupHandlesKHR(_device->vulkanDevice(), raytracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

      const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
      const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

      _raygenShaderBindingTable = new VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);
      _missShaderBindingTable = new VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);
      _hitShaderBindingTable = new VulkanBuffer(_device, bufferUsageFlags, memoryUsageFlags, handleSize);

      // Copy handles
      _raygenShaderBindingTable->map();
      _missShaderBindingTable->map();
      _hitShaderBindingTable->map();
      memcpy(_raygenShaderBindingTable->_mapped, shaderHandleStorage.data(), handleSize);
      memcpy(_missShaderBindingTable->_mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
      memcpy(_hitShaderBindingTable->_mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);

      _raygenShaderSbtEntry.deviceAddress = _raygenShaderBindingTable->deviceAddress();
      _raygenShaderSbtEntry.stride = handleSizeAligned;
      _raygenShaderSbtEntry.size = handleSizeAligned;

      _missShaderSbtEntry.deviceAddress = _missShaderBindingTable->deviceAddress();
      _missShaderSbtEntry.stride = handleSizeAligned;
      _missShaderSbtEntry.size = handleSizeAligned;

      _hitShaderSbtEntry.deviceAddress = _hitShaderBindingTable->deviceAddress();
      _hitShaderSbtEntry.stride = handleSizeAligned;
      _hitShaderSbtEntry.size = handleSizeAligned;
   }

   const VkStridedDeviceAddressRegionKHR& ShaderBindingTable::raygenEntry(void) const
   {
      return _raygenShaderSbtEntry;
   }
   const VkStridedDeviceAddressRegionKHR& ShaderBindingTable::missEntry(void) const
   {
      return _missShaderSbtEntry;
   }
   const VkStridedDeviceAddressRegionKHR& ShaderBindingTable::hitEntry(void) const
   {
      return _hitShaderSbtEntry;
   }
   const VkStridedDeviceAddressRegionKHR& ShaderBindingTable::callableEntry(void) const
   {
      return _callableShaderSbtEntry;
   }

   const std::vector<VkPipelineShaderStageCreateInfo>& ShaderBindingTable::shaderStages() const
   {
      return _shaderStages;
   }

   const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& ShaderBindingTable::shaderGroups() const
   {
      return _shaderGroups;
   }
}