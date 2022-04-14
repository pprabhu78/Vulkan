#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/gltfMaterial.h"
#include "../common/gltfModelDesc.h"
#include "input_output.h"

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormalViewSpace;
layout (location = 3) in vec3 inVertexViewSpace;
layout (location = 4) flat in uint inDrawIndex;
layout (location = 5) flat in uint inModelId;

layout (location = 0) out vec4 outFragColor;

void main() 
{
#if INDIRECT
	const Model model = models._models[inModelId];

	IndexIndicesBuffer indexIndicesBuffer = IndexIndicesBuffer(model.indexIndicesAddress);
	MaterialBuffer  materialBuffer   = MaterialBuffer(model.materialAddress);
	MaterialIndicesBuffer  materialIndicesBuffer   = MaterialIndicesBuffer(model.materialIndicesAddress);

	const uint64_t textureOffset = model.textureOffset;

	const Material material = materialBuffer._materials[materialIndicesBuffer._materialIndices[inDrawIndex]];
	const uint samplerIndex = uint(textureOffset) + material.baseColorTextureIndex;
	
	vec4 colorFromTexture = (samplerIndex==-1)? vec4(1,1,1,1) : texture(samplers[samplerIndex], inUV);
	vec4 color = colorFromTexture * vec4(material.baseColorFactor.xyz,1) * vec4(inColor.xyz, 1) * dot(normalize(inNormalViewSpace), normalize(-inVertexViewSpace));
#else
	vec4 color = texture(samplerColor, inUV) * vec4(material.baseColorFactor.xyz,1) * vec4(inColor.xyz, 1) * dot(normalize(inNormalViewSpace), normalize(-inVertexViewSpace));
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