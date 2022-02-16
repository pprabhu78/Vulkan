#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

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
    vec3 sampleCoords = normalize(payLoad.rayDirection);
    sampleCoords.xy *= pushConstants.environmentMapCoordTransform.xy;
    payLoad.hitValue = texture(environmentMap, sampleCoords).xyz;
#endif
}