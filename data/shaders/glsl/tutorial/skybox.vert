#version 450

#extension GL_GOOGLE_include_directive : enable

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
