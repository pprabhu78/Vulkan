#if CPU_SIDE_COMPILATION
using namespace glm;
#else
#endif

#define Viz_None              0 
#define Viz_Albedo            1
#define Viz_Emissive          2
#define Viz_Roughness         3
#define Viz_Metalness         4
#define Viz_Occlusion         5

#define Viz_NormalMap         6
#define Viz_GeometryNormals   7
#define Viz_NormalNormals     8

struct PushConstants
{
   vec4 clearColor;
   vec4 environmentMapCoordTransform;
   int frameIndex;
   float textureLodBias;
   float reflectivity;
   float contributionFromEnvironment;
   int pathTracer;
   int materialComponentViz;
   int cosineSampling;
   int maxBounces;

#if CPU_SIDE_COMPILATION
   PushConstants()
   {
      environmentMapCoordTransform = vec4(1, 1, 1, 1);

      frameIndex = -1;
      textureLodBias = 0;
      reflectivity = 0;

      pathTracer = 1;

      materialComponentViz = 0;

      cosineSampling = 1;

      maxBounces = 10;
   }
#endif
};

struct SceneUbo
{
   mat4 viewMatrix;
   mat4 projectionMatrix;

   mat4 viewMatrixInverse;
   mat4 projectionMatrixInverse;

   int vertexSizeInBytes;
};
