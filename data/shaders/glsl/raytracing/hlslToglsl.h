#ifndef HLSL_TO_GLSL_H
#define HLSL_TO_GLSL_H

// defines pre-processor macros to easily allow to use hlsl code directly
#define float4 vec4 
#define float3 vec3  
#define float2 vec2 
#define rsqrt inversesqrt
#define lerp mix

float saturate(float val)
{
   return clamp(val, 0.0f, 1.0f);
}

vec2 saturate(vec2 vec)
{
   return clamp(vec, 0.0f, 1.0f);
}

vec3 saturate(vec3 vec)
{
   return clamp(vec, 0.0f, 1.0f);
}

vec4 saturate(vec4 vec)
{
   return clamp(vec, 0.0f, 1.0f);
}

#endif