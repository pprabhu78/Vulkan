
// switch between path tracer and N dot V lighting
#define PATH_TRACER 1

struct HitPayload
{
   vec3 hitValue;
   uint seed;
   vec3 rayOrigin;
   vec3 rayDirection;
   uint depth;
   vec3 weight;
};

struct Vertex
{
   vec3 pos;
   vec3 normal;
   vec2 uv;
   vec4 color;
};

#include "../common/gltfMaterial.h"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba32f) uniform image2D intermediateImage;
layout(set = 0, binding = 2, rgba8) uniform image2D finalImage;
layout(set = 0, binding = 3) uniform UBO
{
   mat4 viewMatrix;
   mat4 viewInverse;
   mat4 projInverse;
   int vertexSizeInBytes;
} sceneUbo;

layout(set = 0, binding = 4) buffer Vertices { vec4 v[]; } vertices;
layout(set = 0, binding = 5) buffer Indices { uint i[]; } indices;


struct PushConstants
{
   vec4 clearColor;
   int frameIndex;
   vec3 pad;
};

layout(push_constant) uniform _PushConstants { PushConstants pushConstants; };


layout(set = 1, binding = 0, std430) readonly buffer materialBuffer
{
   Material _materialBuffer[];
};

layout(set = 1, binding = 1, std430) readonly buffer materialIndices
{
   uint _materialIndices[];
};
layout(set = 1, binding = 2, std430) readonly buffer indexIndices
{
   uint _indexIndices[];
};

layout(set = 1, binding = 3) uniform sampler2D samplers[];


