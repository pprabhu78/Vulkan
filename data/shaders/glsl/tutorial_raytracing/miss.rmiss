#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "rayTracingInputOutput.h"

layout(location = 0) rayPayloadInEXT HitPayload payLoad;

void main()
{
#if PATH_TRACER
    if(payLoad.depth == 0)
    {
        payLoad.hitValue = pushConstants.clearColor.xyz * 0.8;
    }
    else
    {
        payLoad.hitValue = vec3(pushConstants.contributionFromEnvironment);  // No contribution from environment
    }
    payLoad.depth = 100;              // Ending trace
#else
    payLoad.hitValue = vec3(0.0f,0.0f,0.2f);
#endif
}