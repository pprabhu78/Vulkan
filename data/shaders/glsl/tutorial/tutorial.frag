#version 450

#define INDIRECT 1

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : enable

#include "../common/gltfMaterial.h"

#if INDIRECT

layout (set = 1, binding = 0, std430) readonly buffer materialBuffer
{
	Material _materialBuffer[];
};

layout (set = 1, binding = 1, std430) readonly buffer materialIndices
{
	uint _materialIndices[];
};
layout (set = 1, binding = 2, std430) readonly buffer indexIndices
{
	uint _indexIndices[];
};

layout (set = 1, binding = 3) uniform sampler2D samplers[];
#else
layout (set = 1, binding = 0) uniform sampler2D samplerColor;
#endif

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormalViewSpace;
layout (location = 3) in vec3 inVertexViewSpace;
layout (location = 4) flat in uint inDrawIndex;

layout (location = 0) out vec4 outFragColor;

void main() 
{
#if INDIRECT
	uint samplerIndex = _materialBuffer[_materialIndices[inDrawIndex]].baseColorTextureIndex;
	
	vec4 color = texture(samplers[samplerIndex], inUV) * vec4(inColor.xyz, 1) * dot(normalize(inNormalViewSpace), normalize(-inVertexViewSpace));
#else
	vec4 color = texture(samplerColor, inUV) * vec4(inColor.xyz, 1) * dot(normalize(inNormalViewSpace), normalize(-inVertexViewSpace));
#endif

	outFragColor = color;	
}