#include "VulkanMeshlet.h"
#include "GenAssert.h"
#include "Buffer.h"
#include "Vertex.h"

#include <fstream>
#include <cassert>

namespace genesis
{
   using namespace Meshlets;

   VulkanMeshletModel::VulkanMeshletModel(Device* device)
      : _device(device)
   {

   }
   VulkanMeshletModel::~VulkanMeshletModel()
   {
      for (auto buffer : _vertexBuffers)
      {
         delete buffer;
      }
      for (auto buffer : _meshletBuffers)
      {
         delete buffer;
      }
      for (auto buffer : _uniqueVertexIndices)
      {
         delete buffer;
      }
      for (auto buffer : _primitiveIndices)
      {
         delete buffer;
      }
   }

   const InputElementDesc c_elementDescs[Attribute::Count] =
   {
   { "POSITION", 0, R32G32B32_FLOAT, 0, 1, -1, 1 },
   { "NORMAL", 0, R32G32B32_FLOAT, 0, 1, -1, 1 },
   { "TEXCOORD", 0, R32G32_FLOAT, 0, 1, -1, 1 },
   { "TANGENT", 0, R32G32B32_FLOAT, 0, 1, -1, 1 },
   { "BITANGENT", 0, R32G32B32_FLOAT, 0, 1, -1, 1 },
   };

   bool VulkanMeshletModel::loadfromFile(const std::string& fileName)
   {
      std::ifstream stream(fileName, std::ios::binary);
      if (!stream.is_open())
      {
         return false;
      }

      std::vector<MeshHeader> meshes;
      std::vector<BufferView> bufferViews;
      std::vector<Accessor> accessors;

      FileHeader header;
      stream.read(reinterpret_cast<char*>(&header), sizeof(header));

      if (header.Prolog != c_prolog)
      {
         return false; // Incorrect file format.
      }

      if (header.Version != CURRENT_FILE_VERSION)
      {
         return false; // Version mismatch between export and import serialization code.
      }

      // Read mesh metdata
      meshes.resize(header.MeshCount);
      stream.read(reinterpret_cast<char*>(meshes.data()), meshes.size() * sizeof(meshes[0]));

      accessors.resize(header.AccessorCount);
      stream.read(reinterpret_cast<char*>(accessors.data()), accessors.size() * sizeof(accessors[0]));

      bufferViews.resize(header.BufferViewCount);
      stream.read(reinterpret_cast<char*>(bufferViews.data()), bufferViews.size() * sizeof(bufferViews[0]));

      m_buffer.resize(header.BufferSize);
      stream.read(reinterpret_cast<char*>(m_buffer.data()), header.BufferSize);

      char eofbyte;
      stream.read(&eofbyte, 1); // Read last byte to hit the eof bit

      assert(stream.eof()); // There's a problem if we didn't completely consume the file contents.

      stream.close();

      // Populate mesh data from binary data and metadata.
      m_meshes.resize(meshes.size());
      for (uint32_t i = 0; i < static_cast<uint32_t>(meshes.size()); ++i)
      {
         auto& meshView = meshes[i];
         auto& mesh = m_meshes[i];

         // Index data
         {
            Accessor& accessor = accessors[meshView.Indices];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.IndexSize = accessor.Size;
            mesh.IndexCount = accessor.Count;

            mesh.Indices = MakeSpan(m_buffer.data() + bufferView.Offset, bufferView.Size);
         }

         // Index Subset data
         {
            Accessor& accessor = accessors[meshView.IndexSubsets];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.IndexSubsets = MakeSpan(reinterpret_cast<Subset*>(m_buffer.data() + bufferView.Offset), accessor.Count);
         }

         // Vertex data & layout metadata

         // Determine the number of unique Buffer Views associated with the vertex attributes & copy vertex buffers.
         std::vector<uint32_t> vbMap;

         mesh.LayoutDesc.pInputElementDescs = mesh.LayoutElems;
         mesh.LayoutDesc.NumElements = 0;

         for (uint32_t j = 0; j < Attribute::Count; ++j)
         {
            if (meshView.Attributes[j] == -1)
               continue;

            Accessor& accessor = accessors[meshView.Attributes[j]];

            auto it = std::find(vbMap.begin(), vbMap.end(), accessor.BufferView);
            if (it != vbMap.end())
            {
               continue; // Already added - continue.
            }

            // New buffer view encountered; add to list and copy vertex data
            vbMap.push_back(accessor.BufferView);
            BufferView& bufferView = bufferViews[accessor.BufferView];

            Span<uint8_t> verts = MakeSpan(m_buffer.data() + bufferView.Offset, bufferView.Size);

            mesh.VertexStrides.push_back(accessor.Stride);
            mesh.Vertices.push_back(verts);
            mesh.VertexCount = static_cast<uint32_t>(verts.size()) / accessor.Stride;
         }

         for (uint32_t j = 0; j < Attribute::Count; ++j)
         {
            if (meshView.Attributes[j] == -1)
               continue;

            Accessor& accessor = accessors[meshView.Attributes[j]];

            auto it = std::find(vbMap.begin(), vbMap.end(), accessor.BufferView);
            if (it != vbMap.end())
            {
               continue; // Already added - continue.
            }

            // New buffer view encountered; add to list and copy vertex data
            vbMap.push_back(accessor.BufferView);
            BufferView& bufferView = bufferViews[accessor.BufferView];

            Span<uint8_t> verts = MakeSpan(m_buffer.data() + bufferView.Offset, bufferView.Size);

            mesh.VertexStrides.push_back(accessor.Stride);
            mesh.Vertices.push_back(verts);
            mesh.VertexCount = static_cast<uint32_t>(verts.size()) / accessor.Stride;
         }

         // Meshlet data
         {
            Accessor& accessor = accessors[meshView.Meshlets];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.Meshlets = MakeSpan(reinterpret_cast<Meshlet*>(m_buffer.data() + bufferView.Offset), accessor.Count);
         }

         // Meshlet Subset data
         {
            Accessor& accessor = accessors[meshView.MeshletSubsets];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.MeshletSubsets = MakeSpan(reinterpret_cast<Subset*>(m_buffer.data() + bufferView.Offset), accessor.Count);
         }

         // Unique Vertex Index data
         {
            Accessor& accessor = accessors[meshView.UniqueVertexIndices];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.UniqueVertexIndices = MakeSpan(m_buffer.data() + bufferView.Offset, bufferView.Size);
         }

         // Primitive Index data
         {
            Accessor& accessor = accessors[meshView.PrimitiveIndices];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.PrimitiveIndices = MakeSpan(reinterpret_cast<PackedTriangle*>(m_buffer.data() + bufferView.Offset), accessor.Count);
         }

         // Cull data
         {
            Accessor& accessor = accessors[meshView.CullData];
            BufferView& bufferView = bufferViews[accessor.BufferView];

            mesh.CullingData = MakeSpan(reinterpret_cast<CullData*>(m_buffer.data() + bufferView.Offset), accessor.Count);
         }
      }

      populateBuffers();

      return true;
   }

   void VulkanMeshletModel::populateBuffers(void)
   {
      // Each mesh has only one vertex buffer
      for (uint32_t i = 0; i < m_meshes.size(); ++i)
      {
         auto& mesh = m_meshes[i];
         GEN_ASSERT(mesh.Vertices.size() == 1);

         std::cout << sizeof(VertexPositionNormal) << std::endl;

         GEN_ASSERT(mesh.Vertices[0].size() == mesh.VertexCount * sizeof(VertexPositionNormal));
      }

      VkBufferUsageFlags bufferFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;


      _vertexBuffers.reserve(m_meshes.size());
      for (uint32_t i = 0; i < m_meshes.size(); ++i)
      {
         auto& mesh = m_meshes[i];

         const size_t sizeInBytes = mesh.Vertices[0].size();
         Buffer* buffer = new Buffer(_device, BT_SBO, (int)sizeInBytes, true, bufferFlags);

         uint8_t* pDstData = (uint8_t*)buffer->stagingBuffer();
         memcpy(pDstData, mesh.Vertices[0].data(), sizeInBytes);

         buffer->syncToGpu(true);

         _vertexBuffers.push_back(buffer);
      }

      _meshletBuffers.reserve(m_meshes.size());
      for (uint32_t i = 0; i < m_meshes.size(); ++i)
      {
         auto& mesh = m_meshes[i];
         const size_t sizeInBytes = mesh.Meshlets.size() * sizeof(Meshlet);

         Buffer* buffer = new Buffer(_device, BT_SBO, (int)sizeInBytes, true, bufferFlags);
         void* pDstData = (uint8_t*)buffer->stagingBuffer();
         memcpy(pDstData, mesh.Meshlets.data(), sizeInBytes);
         buffer->syncToGpu(true);

         _meshletBuffers.push_back(buffer);
      }

      _uniqueVertexIndices.reserve(m_meshes.size());
      for (uint32_t i = 0; i < m_meshes.size(); ++i)
      {
         auto& mesh = m_meshes[i];
         const size_t sizeInBytes = mesh.UniqueVertexIndices.size();

         Buffer* buffer = new Buffer(_device, BT_SBO, (int)sizeInBytes, true, bufferFlags);
         void* pDstData = (uint8_t*)buffer->stagingBuffer();
         memcpy(pDstData, mesh.UniqueVertexIndices.data(), sizeInBytes);
         buffer->syncToGpu(true);

         _uniqueVertexIndices.push_back(buffer);
      }

      _primitiveIndices.reserve(m_meshes.size());
      for (uint32_t i = 0; i < m_meshes.size(); ++i)
      {
         auto& mesh = m_meshes[i];
         const size_t sizeInBytes = mesh.PrimitiveIndices.size() * sizeof(uint32_t);

         Buffer* buffer = new Buffer(_device, BT_SBO, (int)sizeInBytes, true, bufferFlags);
         void* pDstData = (uint8_t*)buffer->stagingBuffer();
         memcpy(pDstData, mesh.PrimitiveIndices.data(), sizeInBytes);
         buffer->syncToGpu(true);

         _primitiveIndices.push_back(buffer);
      }
   }

   const std::vector<Buffer*>& VulkanMeshletModel::vertexBuffers()
   {
      return _vertexBuffers;
   }
   const std::vector<Buffer*>& VulkanMeshletModel::meshletBuffers()
   {
      return _meshletBuffers;
   }
   const std::vector<Buffer*>& VulkanMeshletModel::uniqueVertexIndices()
   {
      return _uniqueVertexIndices;
   }
   const std::vector<Buffer*>& VulkanMeshletModel::primitiveIndices()
   {
      return _primitiveIndices;
   }
   const std::vector<Mesh>& VulkanMeshletModel::meshes(void)
   {
      return m_meshes;
   }
}