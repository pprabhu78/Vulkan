#version 450

#extension GL_GOOGLE_include_directive : enable

#include "input_output.h"

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = texture(environmentMap, inUVW);
}