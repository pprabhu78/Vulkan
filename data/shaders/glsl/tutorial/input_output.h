
#if CPU_SIDE_COMPILATION
using namespace glm;
struct SceneUbo
{
public:
#else
layout(set = 0, binding = 0) uniform SceneUbo
{
#endif

   mat4 viewMatrix;
   mat4 projectionMatrix;
   mat4 viewMatrixInverse;
   mat4 projectionMatrixInverse;
#if CPU_SIDE_COMPILATION
};
#else
} sceneUbo;
#endif

#if CPU_SIDE_COMPILATION
#else
layout(set = 0, binding = 1) uniform samplerCube environmentMap;
#endif

struct PushConstants
{
   vec4 clearColor;
   vec4 environmentMapCoordTransform;
   int frameIndex;
   float textureLodBias;
   float reflectivity;
   float pad;
#if CPU_SIDE_COMPILATION
public:
   PushConstants()
   {
      clearColor = glm::vec4(0, 0, 0, 0);
      environmentMapCoordTransform = glm::vec4(1, 1, 1, 1);

      frameIndex = 0;
      textureLodBias = 0;
      reflectivity = 0;
   }
#else
#endif
};

#if CPU_SIDE_COMPILATION
#else
layout(push_constant) uniform _PushConstants { PushConstants pushConstants; };
#endif
