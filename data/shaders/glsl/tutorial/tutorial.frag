#version 450

#define INDIRECT 1

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : enable

#include "../common/gltfMaterial.h"
#include "input_output.h"

#if INDIRECT

layout (set = 1, binding = 0, std430) readonly buffer materialBuffer
{
	Material _materialBuffer[];
};

layout (set = 1, binding = 1, std430) readonly buffer materialIndices
{
	uint _materialIndices[];
};
layout (set = 1, binding = 2) uniform sampler2D samplers[];
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

    if(pushConstants.reflectivity>0)
	{
		// reflect the incident vector about the surface normal
		vec3 incidentVector = normalize(inVertexViewSpace.xyz);
		vec3 reflectedVector = reflect(incidentVector, inNormalViewSpace);
		// convert this into world space
		vec3 reflectedVectorWorldSpace = normalize(vec3(sceneUbo.viewMatrixInverse * vec4(reflectedVector, 0)));

		reflectedVectorWorldSpace.xy *= pushConstants.environmentMapCoordTransform.xy;

		// use the lod to sample an lod to simulate roughness
		float maxLod = floor(log2(textureSize(environmentMap, 0))).x;
		float lod = pushConstants.textureLodBias * maxLod;
		vec4 reflectedColor = texture(environmentMap, reflectedVectorWorldSpace.xyz, lod);

		color = mix(color, reflectedColor, pushConstants.reflectivity);
	}
	
	outFragColor = color;	
}