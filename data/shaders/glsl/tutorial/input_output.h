
layout(set = 0, binding = 0) uniform UBO
{
   mat4 viewMatrix;
   mat4 projectionMatrix;
} sceneUbo;

layout(set = 0, binding = 1) uniform samplerCube environmentMap;


struct PushConstants
{
   vec4 clearColor;
   vec4 environmentMapCoordTransform;
   int frameIndex;
   float textureLodBias;
   float contributionFromEnvironment;
   float pad;
};

layout(push_constant) uniform _PushConstants { PushConstants pushConstants; };
