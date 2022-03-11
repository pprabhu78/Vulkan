#version 460

#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_ray_tracing : enable

#extension GL_EXT_nonuniform_qualifier : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "sampling.glsl"
#include "rayTracingInputOutput.h"

layout(location = 0) rayPayloadInEXT HitPayload payLoad;
hitAttributeEXT vec2 attribs;

void main()
{
	payLoad.hitT = gl_HitTEXT;
	payLoad.instanceCustomIndex = gl_InstanceCustomIndexEXT;
	payLoad.geometryIndex = gl_GeometryIndexEXT;
	payLoad.primitiveID = gl_PrimitiveID;
	payLoad.objectToWorld  = gl_ObjectToWorldEXT;
	payLoad.worldToObject = gl_WorldToObjectEXT;
	payLoad.attribs = attribs;
}
