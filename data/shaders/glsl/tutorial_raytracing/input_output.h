

#include "../common/sceneUbo.h"

#if CPU_SIDE_COMPILATION
#else

layout(set = 0, binding = 0) uniform SceneUboBuffer
{
   SceneUbo sceneUbo;
};
layout(set = 0, binding = 1) uniform samplerCube environmentMap;
layout(push_constant) uniform _PushConstants { PushConstants pushConstants; };
#endif

#if CPU_SIDE_COMPILATION
#else

#define INDIRECT 1

#if INDIRECT

struct Model
{
   uint64_t textureOffset;
   uint64_t vertexBufferAddress;
   uint64_t indexBufferAddress;
   uint64_t indexIndicesAddress;
   uint64_t materialAddress;
   uint64_t materialIndicesAddress;
};

layout(buffer_reference, scalar) buffer VertexBuffer { vec4 _vertices[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { uint _indices[]; };
layout(buffer_reference, scalar) buffer IndexIndicesBuffer { uint _indexIndices[]; };
layout(buffer_reference, scalar) buffer MaterialBuffer { Material _materials[]; };
layout(buffer_reference, scalar) buffer MaterialIndicesBuffer { uint _materialIndices[]; };

layout(set = 1, binding = 0, scalar) buffer ModelBuffer
{
   Model _models[];
} models;

struct Instance
{
   mat4 _xform;
   int _instanceId;
   int _modelId;
   int pad0;
   int pad1;
};

layout(set = 1, binding = 1, std430) readonly buffer InstancesBuffer
{
   Instance _instances[];
};

layout(set = 1, binding = 2) uniform sampler2D samplers[];
#else
layout(set = 1, binding = 0) uniform sampler2D samplerColor;
#endif

#endif