#ifndef MATH_H
#define MATH_H

#include "hlslToglsl.h"

// Calculates rotation quaternion from input vector to the vector (0, 0, 1)
// Input vector must be normalized!
float4 getRotationToZAxis(float3 inVec)
{
   // Handle special case when input is exact or near opposite of (0, 0, 1)
   if (inVec.z < -0.99999f) return float4(1.0f, 0.0f, 0.0f, 0.0f);

   return normalize(float4(inVec.y, -inVec.x, 0.0f, 1.0f + inVec.z));
}

// Optimized point rotation using quaternion
// Source: https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
float3 rotatePoint(float4 q, float3 v)
{
   const float3 qAxis = float3(q.x, q.y, q.z);
   return 2.0f * dot(qAxis, v) * qAxis + (q.w * q.w - dot(qAxis, qAxis)) * v + 2.0f * q.w * cross(qAxis, v);
}

// Returns the quaternion with inverted rotation
float4 invertRotation(float4 q)
{
   return float4(-q.x, -q.y, -q.z, q.w);
}

float luminance(vec3 rgb)
{
   return dot(rgb, vec3(0.2126f, 0.7152f, 0.0722f));
}

#endif
