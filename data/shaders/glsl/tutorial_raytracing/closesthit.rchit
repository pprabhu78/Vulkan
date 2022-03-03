#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_GOOGLE_include_directive : enable

// This is needed to support buffer_reference extension
// We need buffer_reference to be able to store multiple Model structs
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "sampling.glsl"
#include "rayTracingInputOutput.h"

layout(location = 0) rayPayloadInEXT HitPayload payLoad;
hitAttributeEXT vec2 attribs;

Vertex unpack(uint index, int vertexSizeInBytes, VertexBuffer vertexBuffer)
{
	// Unpack the vertices from the SSBO using the glTF vertex structure
	// The multiplier is the size of the vertex divided by four float components (=16 bytes)
	const int m =  vertexSizeInBytes/ 16;

	const vec4 d0 = vertexBuffer._vertices[m * index + 0];
	const vec4 d1 = vertexBuffer._vertices[m * index + 1];
	const vec4 d2 = vertexBuffer._vertices[m * index + 2];

	Vertex v;
	v.pos = d0.xyz;
	v.normal = vec3(d0.w, d1.x, d1.y);
	v.uv = vec2(d1.z, d1.w);
	v.color = vec4(d2.x, d2.y, d2.z, 1.0);
	return v;

	return v;
}

void main()
{
	const Model model = models._models[gl_InstanceCustomIndexEXT];

	VertexBuffer vertexBuffer = VertexBuffer(model.vertexBufferAddress);
	IndexBuffer indexBuffer = IndexBuffer(model.indexBufferAddress);

	IndexIndicesBuffer indexIndicesBuffer = IndexIndicesBuffer(model.indexIndicesAddress);
	MaterialBuffer  materialBuffer   = MaterialBuffer(model.materialAddress);
	MaterialIndicesBuffer  materialIndicesBuffer   = MaterialIndicesBuffer(model.materialIndicesAddress);

	const uint64_t textureOffset = model.textureOffset;

    const uint indicesOffset = indexIndicesBuffer._indexIndices[gl_GeometryIndexEXT];
	const ivec3 index = ivec3(indexBuffer._indices[indicesOffset + (3 * gl_PrimitiveID)], indexBuffer._indices[ indicesOffset + (3 * gl_PrimitiveID + 1)], indexBuffer._indices[ indicesOffset + (3 * gl_PrimitiveID + 2)]);

	Vertex v0 = unpack(index.x, sceneUbo.vertexSizeInBytes, vertexBuffer);
	Vertex v1 = unpack(index.y, sceneUbo.vertexSizeInBytes, vertexBuffer);
	Vertex v2 = unpack(index.z, sceneUbo.vertexSizeInBytes, vertexBuffer);

	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

	const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	const vec3 position = v0.pos * barycentricCoords.x + v1.pos * barycentricCoords.y + v2.pos * barycentricCoords.z;
	const vec2 uv = v0.uv * barycentricCoords.x + v1.uv * barycentricCoords.y + v2.uv * barycentricCoords.z;
	const vec4 vertexColor = v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z;

	const Material material = materialBuffer._materials[materialIndicesBuffer._materialIndices[gl_GeometryIndexEXT]];
	const uint samplerIndex = uint(textureOffset) + material.baseColorTextureIndex;

	vec3 emissive = material.emissiveFactor.xyz;
	if (material.emissiveTextureIndex != -1)
	{
		const uint emissiveSamplerIndex = uint(textureOffset) + material.emissiveTextureIndex;
		emissive *= texture(samplers[emissiveSamplerIndex], uv).xyz;
	}

	float occlusion = 1.0; // no occlusion
	float roughness = material.roughness;
	float metalness = material.metalness;
	if (material.occlusionRoughnessMetalnessTextureIndex != -1)
	{
		const uint ormTextureIndex = uint(textureOffset) + material.occlusionRoughnessMetalnessTextureIndex;
		const vec3 omr = texture(samplers[ormTextureIndex], uv).rgb;

		occlusion *= omr.r;		
		roughness *= omr.g;
		metalness *= omr.b;
	}

	vec3 normalMapNormals = vec3(1,1,1);
	if (material.normalTextureIndex != -1)
	{
		const uint normalTextureIndex = uint(textureOffset) + material.normalTextureIndex;
		normalMapNormals = texture(samplers[normalTextureIndex], uv).rgb;
	}

	if (pushConstants.pathTracer > 0)
	{
		// pick a random position around this tbn
		const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
		const vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

		vec3 worldTangent, worldBiNormal;
		createCoordinateSystem(worldNormal, worldTangent, worldBiNormal);
	
		vec3 rayOrigin = worldPosition;
		vec3 rayDirection = samplingHemisphere(payLoad.seed, worldTangent, worldBiNormal, worldNormal);

	#if 0
		vec3 decal = texture(samplers[samplerIndex], uv).rgb * material.baseColorFactor.xyz * vertexColor.xyz;
	#else
		float maxLod = floor(log2(textureSize(samplers[samplerIndex], 0))).x;
		float lod = pushConstants.textureLodBias * maxLod;
		vec3 decal = textureLod(samplers[samplerIndex], uv, lod).rgb * material.baseColorFactor.xyz * vertexColor.xyz;
	#endif

		payLoad.rayOrigin = rayOrigin;
		payLoad.rayDirection = rayDirection;
		payLoad.hitValue = emissive;

		float cosTheta = dot(rayDirection, worldNormal);
		payLoad.weight = decal * cosTheta;
	}
	else
	{
		vec3 normalViewSpace = (transpose(inverse(sceneUbo.viewMatrix))*vec4(normal.x,normal.y, normal.z, 1)).xyz;
		vec3 vertexViewSpace = (sceneUbo.viewMatrix * vec4(position, 1.0)).xyz;

		// the base color is decreasing the lighting to a very low value.
		// therefore, exclude it for now
		vec3 decal = texture(samplers[samplerIndex], uv).rgb /** material.baseColorFactor.xyz*/ * vertexColor.rgb;

		vec3 lit = vec3(dot(normalize(normalViewSpace), normalize(-vertexViewSpace)));

		vec3 final = decal*lit;
		if(pushConstants.reflectivity>0)
		{
			vec3 incidentVector = normalize(vertexViewSpace);
			vec3 reflectedVector = reflect(incidentVector, normalViewSpace);
			vec3 reflectedVectorWorldSpace = normalize(vec3(sceneUbo.viewMatrixInverse * vec4(reflectedVector, 0)));

			// use the lod to sample an lod to simulate roughness
			float maxLod = floor(log2(textureSize(environmentMap, 0))).x;
			float lod = pushConstants.textureLodBias * maxLod;
			vec3 reflectedColor = textureLod(environmentMap, reflectedVectorWorldSpace.xyz*vec3(pushConstants.environmentMapCoordTransform.xy,1), lod).xyz;
			final = mix(final, reflectedColor, pushConstants.reflectivity);
		}
		payLoad.hitValue = final;

		if (pushConstants.materialComponentViz>0)
		{
			if (pushConstants.materialComponentViz==Viz_Albedo)
			{
				payLoad.hitValue = decal;
			}
			else if (pushConstants.materialComponentViz==Viz_Emissive)
			{
				payLoad.hitValue = emissive;
			}
			else if (pushConstants.materialComponentViz==Viz_Roughness)
			{
				payLoad.hitValue = vec3(roughness);
			}
			else if (pushConstants.materialComponentViz==Viz_Metalness)
			{
				payLoad.hitValue = vec3(metalness);
			}
			else if (pushConstants.materialComponentViz==Viz_Occlusion)
			{
				payLoad.hitValue = vec3(occlusion);
			}
			else if (pushConstants.materialComponentViz==Viz_NormalMap)
			{
				payLoad.hitValue = normalMapNormals;
			}
		}
	}
}
