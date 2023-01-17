#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "Span.h"
#include "GenMath.h"

// This code is picked up from
// https://learn.microsoft.com/en-us/samples/microsoft/directx-graphics-samples/d3d12-mesh-shader-samples-win32/
// It loads file that are converted using the first example there: Wavefront Converter Command Line Tool
// It needs to run on an obj file like so: d:\temp\lucy.obj -a pn
// The input obj file must have face normals
namespace genesis
{

   class Device;
   class Buffer;

   namespace Meshlets
   {
      const uint32_t c_sizeMap[] =
      {
      12, // Position
      12, // Normal
      8,  // TexCoord
      12, // Tangent
      12, // Bitangent
      };

      struct Attribute
      {
         enum EType : uint32_t
         {
            Position,
            Normal,
            TexCoord,
            Tangent,
            Bitangent,
            Count
         };

         EType    Type;
         uint32_t Offset;
      };

      struct Subset
      {
         uint32_t Offset;
         uint32_t Count;
      };

      struct MeshInfo
      {
         uint32_t IndexSize;
         uint32_t MeshletCount;

         uint32_t LastMeshletVertCount;
         uint32_t LastMeshletPrimCount;
      };

      struct Meshlet
      {
         uint32_t VertCount;
         uint32_t VertOffset;
         uint32_t PrimCount;
         uint32_t PrimOffset;
      };

      struct PackedTriangle
      {
         uint32_t i0 : 10;
         uint32_t i1 : 10;
         uint32_t i2 : 10;
      };

      struct CullData
      {
         Vector4_32 BoundingSphere; // xyz = center, w = radius
         uint8_t           NormalCone[4];  // xyz = axis, w = -cos(a + 90)
         float             ApexOffset;     // apex = center - axis * offset
      };

      const uint32_t c_prolog = 'MSHL';

      enum FileVersion
      {
         FILE_VERSION_INITIAL = 0,
         CURRENT_FILE_VERSION = FILE_VERSION_INITIAL
      };

      struct FileHeader
      {
         uint32_t Prolog;
         uint32_t Version;

         uint32_t MeshCount;
         uint32_t AccessorCount;
         uint32_t BufferViewCount;
         uint32_t BufferSize;
      };

      struct MeshHeader
      {
         uint32_t Indices;
         uint32_t IndexSubsets;
         uint32_t Attributes[5];

         uint32_t Meshlets;
         uint32_t MeshletSubsets;
         uint32_t UniqueVertexIndices;
         uint32_t PrimitiveIndices;
         uint32_t CullData;
      };

      struct BufferView
      {
         uint32_t Offset;
         uint32_t Size;
      };

      struct Accessor
      {
         uint32_t BufferView;
         uint32_t Offset;
         uint32_t Size;
         uint32_t Stride;
         uint32_t Count;
      };

      enum InputFormat
      {
         R32G32B32_FLOAT = 0, R32G32_FLOAT = 1
      };

      typedef struct InputElementDesc
      {
         const char* SemanticName;
         unsigned int SemanticIndex;
         InputFormat Format;
         unsigned int InputSlot;
         unsigned int AlignedByteOffset;
         int InputSlotClass;
         unsigned int InstanceDataStepRate;
      } InputElementDesc;

      typedef struct InputLayoutDesc
      {
         const InputElementDesc* pInputElementDescs;
         unsigned int NumElements;
      } 	InputLayoutDesc;

      struct Mesh
      {
         InputElementDesc   LayoutElems[Attribute::Count];
         InputLayoutDesc    LayoutDesc;

         std::vector<Span<uint8_t>> Vertices;
         std::vector<uint32_t>      VertexStrides;
         uint32_t                   VertexCount;
         Vector4_32                 BoundingSphere;

         Span<Subset>               IndexSubsets;
         Span<uint8_t>              Indices;
         uint32_t                   IndexSize;
         uint32_t                   IndexCount;

         Span<Subset>               MeshletSubsets;
         Span<Meshlet>              Meshlets;
         Span<uint8_t>              UniqueVertexIndices;
         Span<PackedTriangle>       PrimitiveIndices;
         Span<CullData>             CullingData;
      };
   }

   class VulkanMeshletModel
   {
   public:
      VulkanMeshletModel(Device* device);
      virtual ~VulkanMeshletModel();

   public:
      virtual bool loadfromFile(const std::string& fileName);

      virtual const std::vector<Buffer*>& vertexBuffers();
      virtual const std::vector<Buffer*>& meshletBuffers();
      virtual const std::vector<Buffer*>& uniqueVertexIndices();
      virtual const std::vector<Buffer*>& primitiveIndices();
      virtual const std::vector<Meshlets::Mesh>& meshes(void);
   protected:
      virtual void populateBuffers(void);
   protected:
      Device* _device;

      std::vector<Meshlets::Mesh>   m_meshes;
      std::vector<uint8_t> m_buffer;

      std::vector<Buffer*> _vertexBuffers;
      std::vector<Buffer*> _meshletBuffers;
      std::vector<Buffer*> _uniqueVertexIndices;
      std::vector<Buffer*> _primitiveIndices;
   };
}