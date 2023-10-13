#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_include_directive : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/gltfMaterial.h"
#include "../common/gltfModelDesc.h"
#include "input_output.h"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormalViewSpace;
layout (location = 3) out vec3 outVertexViewSpace;
layout (location = 4) out vec3 outLightDirViewSpace;
layout (location = 5) flat out uint outDrawIndex;
layout (location = 6) flat out uint outModelId;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	outColor = inColor;
	outUV = inUV;

	outNormalViewSpace = (transpose(inverse(sceneUbo.viewMatrix))*vec4(inNormal.x,inNormal.y, inNormal.z, 1)).xyz;

	outLightDirViewSpace = (transpose(inverse(sceneUbo.viewMatrix))*vec4(0, 1, 0, 1)).xyz;
	
	outVertexViewSpace = (sceneUbo.viewMatrix * vec4(inPosition, 1.0)).xyz;
	
	const Instance instance = _instances[gl_InstanceIndex];
	const mat4 xform = instance._xform;

	gl_Position = sceneUbo.projectionMatrix * sceneUbo.viewMatrix * xform * vec4(inPosition, 1.0);

	outDrawIndex = gl_DrawIDARB;
	outModelId = instance._modelId;
}
