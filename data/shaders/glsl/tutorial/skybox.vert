#version 450

#extension GL_GOOGLE_include_directive : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/gltfMaterial.h"
#include "../common/gltfModelDesc.h"
#include "input_output.h"

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	outUVW.xy *= pushConstants.environmentMapCoordTransform.xy;

	mat4 modifiedViewMatrix = sceneUbo.viewMatrix;

	modifiedViewMatrix[3][0] = 0;
	modifiedViewMatrix[3][1] = 0;
	modifiedViewMatrix[3][2] = 0;
	modifiedViewMatrix[3][3] = 1;
	gl_Position = sceneUbo.projectionMatrix * modifiedViewMatrix * vec4(inPos.xyz, 1.0);
}
