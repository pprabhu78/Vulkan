#include "Blas.h"
#include "VulkanGltf.h"
#include "VulkanInitializers.h"
#include "Buffer.h"
#include "Device.h"
#include "AccelerationStructure.h"
#include "VulkanExtensions.h"

#include <vector>
#include <deque>

namespace genesis
{
   Blas::Blas(Device* device, const VulkanGltfModel* model)
      : _device(device)
      , _model(model)
   {
      build();
   }

   Blas::~Blas()
   {
      delete _blas;
   }

   void Blas::build()
   {
      VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
      VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

      vertexBufferDeviceAddress.deviceAddress = _model->vertexBuffer()->bufferAddress();
      indexBufferDeviceAddress.deviceAddress = _model->indexBuffer()->bufferAddress();

      VkAccelerationStructureGeometryTrianglesDataKHR    triangles{};
      triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
      triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
      triangles.vertexData = vertexBufferDeviceAddress;
      triangles.vertexStride = (uint32_t)sizeof(Vertex);
      triangles.maxVertex = (uint32_t)_model->numVertices();
      triangles.indexType = VK_INDEX_TYPE_UINT32;
      triangles.indexData = indexBufferDeviceAddress;

      VkAccelerationStructureGeometryKHR accelerationStructureGeometry = vkInitaliazers::accelerationStructureGeometryKHR();
      accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
      accelerationStructureGeometry.geometry.triangles = triangles;

      VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};

      std::vector<VkAccelerationStructureGeometryKHR> vecAccelerationStructureGeometries;
      std::vector<VkAccelerationStructureBuildRangeInfoKHR> vecAccelerationStructureBuildRangeInfos;
      std::vector<uint32_t> primitiveCounts;

      _model->forEachPrimitive(
         [&](const Primitive& primitive)
         {
            accelerationStructureBuildRangeInfo.primitiveCount = primitive.indexCount / 3;
            accelerationStructureBuildRangeInfo.primitiveOffset = primitive.firstIndex * sizeof(uint32_t);
            accelerationStructureBuildRangeInfo.firstVertex = 0;
            accelerationStructureBuildRangeInfo.transformOffset = 0;

            vecAccelerationStructureGeometries.push_back(accelerationStructureGeometry);
            vecAccelerationStructureBuildRangeInfos.push_back(accelerationStructureBuildRangeInfo);
            primitiveCounts.push_back(accelerationStructureBuildRangeInfo.primitiveCount);
         }
      );

      // Get size info
      VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = vkInitaliazers::accelerationStructureBuildGeometryInfoKHR();
      accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationStructureBuildGeometryInfo.geometryCount = (uint32_t)vecAccelerationStructureGeometries.size();
      accelerationStructureBuildGeometryInfo.pGeometries = vecAccelerationStructureGeometries.data();

      VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = vkInitaliazers::accelerationStructureBuildSizesInfoKHR();
      _device->extensions().vkGetAccelerationStructureBuildSizesKHR(
         _device->vulkanDevice(),
         VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
         &accelerationStructureBuildGeometryInfo,
         primitiveCounts.data(),
         &accelerationStructureBuildSizesInfo);

      _blas = new AccelerationStructure(_device, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, accelerationStructureBuildSizesInfo.accelerationStructureSize);

      VkBufferUsageFlags flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

      Buffer* scratchBuffer = new Buffer(_device, BT_SBO, (int)accelerationStructureBuildSizesInfo.buildScratchSize, false, flags);

      VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = vkInitaliazers::accelerationStructureBuildGeometryInfoKHR();
      accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      accelerationBuildGeometryInfo.dstAccelerationStructure = _blas->handle();
      accelerationBuildGeometryInfo.geometryCount = (uint32_t)vecAccelerationStructureGeometries.size();
      accelerationBuildGeometryInfo.pGeometries = vecAccelerationStructureGeometries.data();
      accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer->bufferAddress();

      std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { vecAccelerationStructureBuildRangeInfos.data() };

      // Build the acceleration structure on the device via a one-time command buffer submission
      // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
      VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
      _device->extensions().vkCmdBuildAccelerationStructuresKHR(
         commandBuffer,
         1,
         &accelerationBuildGeometryInfo
         , accelerationBuildStructureRangeInfos.data());
      _device->flushCommandBuffer(commandBuffer);

      delete scratchBuffer;
   }

   uint64_t Blas::deviceAddress(void) const
   {
      return _blas->deviceAddress();
   }
}