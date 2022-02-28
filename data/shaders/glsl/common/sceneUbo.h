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
   int pathTracer;
   vec3 pad;
};

struct SceneUbo
{
   mat4 viewMatrix;
   mat4 projectionMatrix;

   mat4 viewMatrixInverse;
   mat4 projectionMatrixInverse;

   int vertexSizeInBytes;
};
