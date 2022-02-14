#include "Tlas.h"
#include "Blas.h"
#include "VulkanGltf.h"
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "VulkanFunctions.h"
#include "Device.h"
#include "InstanceContainer.h"
#include "ModelRegistry.h"

#include <iostream>

namespace genesis
{
   Tlas::Tlas(Device* device, const ModelRegistry* modelRegistry)
      : _device(device)
      , _modelRegistry(modelRegistry)
   {
      
   }
   Tlas::~Tlas()
   {
      for (auto& keyVal : _mapModelToBlas)
      {
         delete keyVal.second;
      }

      delete _tlas;
   }

   void Tlas::addInstance(const Instance& instance)
   {
      const int modelId = instance._modelId;

      auto it = _mapModelToBlas.find(modelId);

      Blas* blas = nullptr;
      if (it == _mapModelToBlas.end())
      {
         const ModelInfo* modelInfo = _modelRegistry->findModel(modelId);
         if (modelInfo == nullptr)
         {
            std::cout << __FUNCTION__ << "warning: " << "modelInfo == nullptr" << std::endl;
            return;
         }

         blas = new Blas(_device, modelInfo->model());
         _mapModelToBlas.insert({ modelId, blas });
      }
      else
      {
         blas = it->second;
      }
      if (!blas)
      {
         std::cout << "Could not find or created blas" << std::endl;
         return;
      }

      VkTransformMatrixKHR vkTransform;
      glm::mat4 incomingTranspose = glm::transpose(instance._xform);
      memcpy(&vkTransform, &incomingTranspose, sizeof(VkTransformMatrixKHR));

      VkAccelerationStructureInstanceKHR vulkanInstance{};
      vulkanInstance.transform = vkTransform;
      vulkanInstance.instanceCustomIndex = 0;
      vulkanInstance.mask = 0xFF;
      vulkanInstance.instanceShaderBindingTableRecordOffset = 0;
      vulkanInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
      vulkanInstance.accelerationStructureReference = blas->deviceAddress();

      _vulkanInstances.push_back(vulkanInstance);
   }

   void Tlas::build()
   {
      // Buffer for instance data
      genesis::VulkanBuffer* instancesBuffer = new genesis::VulkanBuffer(_device
         , VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
         , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
         , _vulkanInstances.size()*sizeof(VkAccelerationStructureInstanceKHR)
         , _vulkanInstances.data());

      VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
      instanceDataDeviceAddress.deviceAddress = instancesBuffer->deviceAddress();

      VkAccelerationStructureGeometryKHR accelerationStructureGeometry {};
      accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
      accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
      accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
      accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
      accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

      // Get size info
      /*
      The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
      */
      VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
      accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
      accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationStructureBuildGeometryInfo.geometryCount = 1;
      accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

      const uint32_t numInstances = (uint32_t)_vulkanInstances.size();

      VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
      accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
      genesis::vkGetAccelerationStructureBuildSizesKHR(
         _device->vulkanDevice(),
         VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
         &accelerationStructureBuildGeometryInfo,
         &numInstances,
         &accelerationStructureBuildSizesInfo);

      _tlas = new genesis::AccelerationStructure(_device, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, accelerationStructureBuildSizesInfo.accelerationStructureSize);

      // Create a small scratch buffer used during build of the top level acceleration structure
      VulkanBuffer* scratchBuffer = new VulkanBuffer(_device
         , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
         , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, accelerationStructureBuildSizesInfo.buildScratchSize);;

      VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
      accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
      accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      accelerationBuildGeometryInfo.dstAccelerationStructure = _tlas->handle();
      accelerationBuildGeometryInfo.geometryCount = 1;
      accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
      accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer->deviceAddress();

      VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
      accelerationStructureBuildRangeInfo.primitiveCount = numInstances;
      accelerationStructureBuildRangeInfo.primitiveOffset = 0;
      accelerationStructureBuildRangeInfo.firstVertex = 0;
      accelerationStructureBuildRangeInfo.transformOffset = 0;
      std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

      // Build the acceleration structure on the device via a one-time command buffer submission
      // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
      VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
      genesis::vkCmdBuildAccelerationStructuresKHR(
         commandBuffer,
         1,
         &accelerationBuildGeometryInfo,
         accelerationBuildStructureRangeInfos.data());
      _device->flushCommandBuffer(commandBuffer);

      delete scratchBuffer;

      delete instancesBuffer;
   }

   const VkAccelerationStructureKHR& Tlas::handle(void) const
   {
      return _tlas->handle();
   }
}