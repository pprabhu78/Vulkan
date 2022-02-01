
// switch between path tracer and N dot V lighting
#define PATH_TRACER 1

#if CPU_SIDE_COMPILATION
using namespace glm;
#else
#endif

struct PushConstants
{
   vec4 clearColor;
   vec4 environmentMapCoordTransform;
   int frameIndex;
   float textureLodBias;
   float reflectivity;
   float contributionFromEnvironment;
};

#if CPU_SIDE_COMPILATION
#else

#include "../common/gltfMaterial.h"

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba32f) uniform image2D intermediateImage;
layout(set = 0, binding = 2, rgba8) uniform image2D finalImage;
#endif

#if CPU_SIDE_COMPILATION
struct SceneUbo
{
public:
#else
layout(set = 0, binding = 3) uniform UBO
{
#endif
   mat4 viewMatrix;
   mat4 viewInverse;
   mat4 projInverse;
   int vertexSizeInBytes;
#if CPU_SIDE_COMPILATION
};
#else
} sceneUbo;
#endif


#if CPU_SIDE_COMPILATION

#else
layout(set = 0, binding = 4) buffer Vertices { vec4 v[]; } vertices;
layout(set = 0, binding = 5) buffer Indices { uint i[]; } indices;
layout(set = 0, binding = 6) uniform samplerCube environmentMap;


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

#endif
