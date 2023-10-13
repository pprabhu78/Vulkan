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
layout (location = 4) in vec3 inLightDirViewSpace;
layout (location = 5) flat in uint inDrawIndex;
layout (location = 6) flat in uint inModelId;

layout (location = 0) out vec4 outFragColor;

const float M_PI = 3.141592653589793;

// same as F_Schlick here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/brdf.glsl
// 
// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// same as D_GGX here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/brdf.glsl
// 
// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
	float alphaRoughnessSq = alphaRoughness * alphaRoughness;
	float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
	return alphaRoughnessSq / (M_PI * f * f);
}

// Crash Course in BRDF Implementation
// https://boksajak.github.io/blog/BRDF, section 4.2
// substitute lambda, then substitute 'a'
float geometricMaskingShadowing(float NdotL, float NdotV, float alphaRoughness)
{
	float alphaRoughnessSq = alphaRoughness * alphaRoughness;
	float gl = 2.0 * NdotL / (NdotL + sqrt(alphaRoughnessSq + (1.0 - alphaRoughnessSq) * (NdotL * NdotL)));
	float gv = 2.0 * NdotV / (NdotV + sqrt(alphaRoughnessSq + (1.0 - alphaRoughnessSq) * (NdotV * NdotV)));
	return gl * gv;
}

// same as BRDF_lambertian here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/brdf.glsl
// 
//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 diffuseColor, vec3 F)
{
	// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	return (1.0 - F) * (diffuseColor / M_PI);
}

vec3 computeLighting(vec3 n, vec3 l, vec3 v
	, float roughness, float metalness, float occlusion
	, vec3 baseColor, vec3 lightColor
	, out vec3 debugValue)
{
	const vec3 h = normalize(l + v);                        // Half vector between both l and v

	const float NdotL = clamp(dot(n, l), 0.001, 1.0);
	const float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	const float NdotH = clamp(dot(n, h), 0.0, 1.0);
	const float LdotH = clamp(dot(l, h), 0.0, 1.0);
	const float VdotH = clamp(dot(v, h), 0.0, 1.0);

	const vec3 f0 = vec3(0.04);

	const vec3 diffuseColor = (baseColor.rgb * (vec3(1.0) - f0)) * (1.0 - metalness);

	const vec3 specularColor = mix(f0, baseColor.rgb, metalness);

	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
	const vec3 reflectance0 = specularColor.rgb;
	const vec3 reflectance90 = vec3(1.0, 1.0, 1.0) * clamp(reflectance * 25.0, 0.0, 1.0);

	const float alphaRoughness = roughness * roughness;

	// fresnel
	const vec3 F = F_Schlick(reflectance0, reflectance90, VdotH);

	// microfacet distribution
	const float D = D_GGX(NdotH, alphaRoughness);

	// geometric masking, shadowing
	const float G = geometricMaskingShadowing(NdotL, NdotV, alphaRoughness);

	vec3 specularContribution = (F * D * G) / (4.0 * NdotL * NdotV);

	vec3 diffuseContribution = BRDF_lambertian(diffuseColor, F);

	vec3 color = NdotL * lightColor * (diffuseContribution + specularContribution);

	debugValue = specularContribution;

	return color;
}

// n and v are in view space
vec3 getIBLContribution(vec3 n, vec3 v, float roughness, float metalness, float occlusion
	, vec3 baseColor
	, samplerCube environmentMap
	, sampler2D brdfLut
	, mat4 envMapTextureMatrix
	, float iblScale
	, out vec3 debugValue)
{
	// use the lod to sample an lod to simulate roughness
	const float maxLod = floor(log2(textureSize(environmentMap, 0))).x;
	const float lod = roughness * maxLod;

	const float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);

	const vec3 f0 = vec3(0.04);

	const vec3 diffuseColor = (baseColor.rgb * (vec3(1.0) - f0)) * (1.0 - metalness);
	const vec3 specularColor = mix(f0, baseColor.rgb, metalness);

	// retrieve a scale and bias to F0. See [1], Figure 3
	const vec3 brdf = (texture(brdfLut, vec2(NdotV, 1.0 - roughness))).rgb;

	vec3 envMapSpaceNormal = normalize((envMapTextureMatrix * vec4(n, 0)).xyz);
	const vec3 diffuseLight = texture(environmentMap, envMapSpaceNormal.xyz, maxLod).rgb;

	vec3 reflection = reflect(-v, n);
	reflection = normalize(reflection);

	vec3 envMapSpaceReflection = normalize((envMapTextureMatrix * vec4(reflection, 0)).xyz);
	const vec3 specularLight = textureLod(environmentMap, envMapSpaceReflection.xyz, lod).rgb;

	vec3 diffuse = diffuseLight * diffuseColor;
	vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

	diffuse *= iblScale;
	specular *= iblScale;

	return diffuse + specular;
}


void main() 
{
	const Model model = models._models[inModelId];

	MaterialBuffer  materialBuffer   = MaterialBuffer(model.materialAddress);
	MaterialIndicesBuffer  materialIndicesBuffer   = MaterialIndicesBuffer(model.materialIndicesAddress);
	const Material material = materialBuffer._materials[materialIndicesBuffer._materialIndices[inDrawIndex]];
	const uint64_t textureOffset = model.textureOffset;
	const uint samplerIndex = uint(textureOffset) + material.baseColorTextureIndex;

	float occlusion = 1.0; // no occlusion
	float roughness = 1;
	float metalness = 1;
	if (material.occlusionRoughnessMetalnessTextureIndex != -1)
	{
		const uint ormTextureIndex = uint(textureOffset) + material.occlusionRoughnessMetalnessTextureIndex;
		const vec3 omr = texture(samplers[ormTextureIndex], inUV).rgb;

		occlusion *= omr.r;
		roughness *= omr.g;
		metalness *= omr.b;
	}
	
	const vec3 colorFromTexture = (samplerIndex == -1) ? vec3(1, 1, 1) : texture(samplers[samplerIndex], inUV).rgb;

	const vec3 baseColor = colorFromTexture * material.baseColorFactor.xyz * inColor;

	vec3 debugValue;

	vec4 color;
	color.rgb = computeLighting(normalize(inNormalViewSpace), inLightDirViewSpace, normalize(-inVertexViewSpace.xyz)
		, roughness, metalness, occlusion, baseColor, vec3(1, 1, 1), debugValue);
	color.a = 1;

	if (pushConstants.reflectivity > 0)
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
	
	outFragColor = vec4(color.r, color.g, color.b, 1);
}