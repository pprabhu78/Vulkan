#include "globals.glsl"

#include "brdf.glsl"
#include "math.glsl"

#define MIN_BOUNCES_FOR_RUSSIAN_ROULETTE 3

layout(location = 0) rayPayloadEXT HitPayload payLoad;

vec3 pathTrace()
{
   vec3 radiance = vec3(0.0);
   return radiance;
}

Vertex unpack(uint index, int vertexSizeInBytes, VertexBuffer vertexBuffer)
{
	// Unpack the vertices from the SSBO using the glTF vertex structure
	// The multiplier is the size of the vertex divided by four float components (=16 bytes)
	const int m = vertexSizeInBytes / 16;

	const vec4 d0 = vertexBuffer._vertices[m * index + 0];
	const vec4 d1 = vertexBuffer._vertices[m * index + 1];
	const vec4 d2 = vertexBuffer._vertices[m * index + 2];

	Vertex v;
	v.position = d0.xyz;
	v.normal = vec3(d0.w, d1.x, d1.y);
	v.uv = vec2(d1.z, d1.w);
	v.color = vec4(d2.x, d2.y, d2.z, 1.0);
	return v;

	return v;
}

Vertex loadVertex(in Model model, out vec3 geometryNormal)
{
	VertexBuffer vertexBuffer = VertexBuffer(model.vertexBufferAddress);
	IndexBuffer indexBuffer = IndexBuffer(model.indexBufferAddress);

	IndexIndicesBuffer indexIndicesBuffer = IndexIndicesBuffer(model.indexIndicesAddress);

	const uint indicesOffset = indexIndicesBuffer._indexIndices[payLoad.geometryIndex];
	const ivec3 index = ivec3(indexBuffer._indices[indicesOffset + (3 * payLoad.primitiveID)], indexBuffer._indices[indicesOffset + (3 * payLoad.primitiveID + 1)], indexBuffer._indices[indicesOffset + (3 * payLoad.primitiveID + 2)]);

	Vertex v0 = unpack(index.x, sceneUbo.vertexSizeInBytes, vertexBuffer);
	Vertex v1 = unpack(index.y, sceneUbo.vertexSizeInBytes, vertexBuffer);
	Vertex v2 = unpack(index.z, sceneUbo.vertexSizeInBytes, vertexBuffer);

	const vec3 barycentricCoords = vec3(1.0f - payLoad.attribs.x - payLoad.attribs.y, payLoad.attribs.x, payLoad.attribs.y);

	Vertex vertex;
	vertex.normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	vertex.position = v0.position * barycentricCoords.x + v1.position * barycentricCoords.y + v2.position * barycentricCoords.z;
	vertex.uv = v0.uv * barycentricCoords.x + v1.uv * barycentricCoords.y + v2.uv * barycentricCoords.z;
	vertex.color = v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z;

	vec3 edge01 = v1.position - v0.position;
	vec3 edge02 = v2.position - v0.position;

	geometryNormal = normalize(cross(edge02, edge01));

	return vertex;
}

MaterialProperties loadMaterialProperties(in Model model, in Vertex vertex)
{
	MaterialBuffer  materialBuffer = MaterialBuffer(model.materialAddress);
	MaterialIndicesBuffer  materialIndicesBuffer = MaterialIndicesBuffer(model.materialIndicesAddress);
	const Material material = materialBuffer._materials[materialIndicesBuffer._materialIndices[payLoad.geometryIndex]];
	const uint64_t textureOffset = model.textureOffset;
	const uint samplerIndex = uint(textureOffset)+material.baseColorTextureIndex;

	vec3 emissive = material.emissiveFactor.xyz;
	if (material.emissiveTextureIndex != -1)
	{
		const uint emissiveSamplerIndex = uint(textureOffset)+material.emissiveTextureIndex;
		emissive *= texture(samplers[emissiveSamplerIndex], vertex.uv).xyz;
	}

	float occlusion = 1.0; // no occlusion
	float roughness = material.roughness;
	float metalness = material.metalness;
	if (material.occlusionRoughnessMetalnessTextureIndex != -1)
	{
		const uint ormTextureIndex = uint(textureOffset)+material.occlusionRoughnessMetalnessTextureIndex;
		const vec3 omr = texture(samplers[ormTextureIndex], vertex.uv).rgb;

		occlusion *= omr.r;
		roughness *= omr.g;
		metalness *= omr.b;
	}

	vec3 normalMapNormals = vec3(1, 1, 1);
	if (material.normalTextureIndex != -1)
	{
		const uint normalTextureIndex = uint(textureOffset)+material.normalTextureIndex;
		normalMapNormals = texture(samplers[normalTextureIndex], vertex.uv).rgb;
	}

	MaterialProperties materialProperties;

#if 0
	materialProperties.baseColor = texture(samplers[samplerIndex], uv).rgb * material.baseColorFactor.xyz * vertexColor.xyz;
#else
	// This can crash if texture size is not a power of 2
	//float maxLod = floor(log2(textureSize(samplers[samplerIndex], 0))).x;
	//float lod = pushConstants.textureLodBias * maxLod;
	const float lod = 0;
	vec3 colorFromTexture = (samplerIndex == -1) ? vec3(1, 1, 1) : textureLod(samplers[samplerIndex], vertex.uv, lod).rgb;
	materialProperties.baseColor = colorFromTexture * material.baseColorFactor.xyz * vertex.color.xyz;
#endif

	materialProperties.metalness = metalness;
	materialProperties.roughness = roughness;
	materialProperties.emissive = emissive;
	materialProperties.occlusion = occlusion;

	return materialProperties;
}

//if (pushConstants.materialComponentViz > 0)
//{
vec3 materialComponentViz(MaterialProperties materialProperties)
{
	if (pushConstants.materialComponentViz == Viz_Albedo)
	{
		return materialProperties.baseColor;
	}
	else if (pushConstants.materialComponentViz == Viz_Emissive)
	{
		return materialProperties.emissive;
	}
	else if (pushConstants.materialComponentViz == Viz_Roughness)
	{
		return vec3(materialProperties.roughness);
	}
	else if (pushConstants.materialComponentViz == Viz_Metalness)
	{
		return vec3(materialProperties.metalness);
	}
	else if (pushConstants.materialComponentViz == Viz_Occlusion)
	{
		return vec3(materialProperties.occlusion);
	}
	else if (pushConstants.materialComponentViz == Viz_NormalMap)
	{
		return vec3(0, 0, 1);
	}
	return vec3(1, 1, 1);

}

Ray calculateRay(const ivec2 imageCoords, const ivec2 imageSize)
{
	// compute the sampling position
	const vec2 pixelCenter = imageCoords + vec2(0.5);
	const vec2 inUV = pixelCenter / imageSize;
	vec2 d = inUV * 2.0 - 1.0;

	// compute the ray origina and direction
	vec4 rayOrigin4 = sceneUbo.viewMatrixInverse * vec4(0, 0, 0, 1);
	vec4 target = sceneUbo.projectionMatrixInverse * vec4(d.x, d.y, 1, 1);
	vec4 rayDirection4 = sceneUbo.viewMatrixInverse * vec4(normalize(target.xyz), 0);

	Ray ray;
	ray.origin = rayOrigin4.xyz;
	ray.direction = rayDirection4.xyz;

	return ray;
}

vec3 pathTrace(const ivec2 imageCoords, const ivec2 imageSize)
{
	Ray ray = calculateRay(imageCoords, imageSize);

	vec3 weight = vec3(0.0);

	vec3 throughput = vec3(1);
	vec3 radiance = vec3(0);

	int maxBounces = pushConstants.maxBounces;
	for (int depth = 0; depth < maxBounces; ++depth)
	{
		payLoad.hitT = INFINITY;
		traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, ray.origin, T_MIN, ray.direction, T_MAX, 0);

		if (payLoad.hitT == INFINITY)
		{
			vec3 sampleCoords = normalize(ray.direction);
			sampleCoords.xy *= pushConstants.environmentMapCoordTransform.xy;
			vec3 env = /*texture(environmentMap, sampleCoords).xyz**/vec3(pushConstants.contributionFromEnvironment);
			return radiance + (env * throughput);
		}

		const Model model = models._models[payLoad.instanceCustomIndex];

		vec3 geometryNormal;
		const Vertex vertex = loadVertex(model, geometryNormal);
		const MaterialProperties materialProperties = loadMaterialProperties(model, vertex);

		const vec3 worldPosition = vec3(payLoad.objectToWorld * vec4(vertex.position, 1.0));
		vec3 worldNormal = normalize(vec3(vertex.normal * payLoad.worldToObject));
		
		vec3 worldTangent, worldBiNormal;
		createCoordinateSystem(worldNormal, worldTangent, worldBiNormal);

		// early out
		if (pushConstants.materialComponentViz > 0)
		{
			return materialComponentViz(materialProperties);
		}

#if 0
		if (rnd(payLoad.seed) > materialProperties.alpha)
		{
			ray.origin = worldPosition;
			continue;
		}
#endif

		// add emissive at this hit
		radiance += materialProperties.emissive * throughput;

		// If we are on the last bounce, we don't need to evaluate the brdf, so just terminate
		if (depth == maxBounces - 1)
		{
			break;
		}

		if (depth > MIN_BOUNCES_FOR_RUSSIAN_ROULETTE)
		{
			float rrProbability = min(0.95f, luminance(throughput));
			if (rrProbability < rnd(payLoad.seed))
			{
				break;
			}
			else
			{
				throughput /= rrProbability;
			}
		}

		Ray currentRay = ray;
		vec3 V = -currentRay.direction;

		vec3 worldGeometryNormal = normalize(vec3(geometryNormal * payLoad.worldToObject));
		if (dot(worldGeometryNormal, V) < 0.0f)
		{
			worldGeometryNormal = -geometryNormal;
		}
		if (dot(worldGeometryNormal, worldNormal) < 0.0f)
		{
			worldNormal = -worldNormal;
		}

		int brdfType = DIFFUSE_TYPE;
		if (materialProperties.metalness == 1.0f && materialProperties.roughness == 0.0f) {
			// Fast path for mirrors
			brdfType = SPECULAR_TYPE;
		}
		else {

			// Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
			float brdfProbability = getBrdfProbability(materialProperties, V, worldNormal);

			if (rnd(payLoad.seed) < brdfProbability) {
				brdfType = SPECULAR_TYPE;
				throughput /= brdfProbability; // modulate throughput by the probability
			}
			else {
				brdfType = DIFFUSE_TYPE;
				throughput /= (1.0f - brdfProbability); // modulate throughput by the probability
			}
		}
		
		vec2 u;
		u.x = rnd(payLoad.seed);
		u.y = rnd(payLoad.seed);

		bool ok = evaluateBrdf(brdfType, pushConstants.cosineSampling, u
			, materialProperties, worldNormal, worldGeometryNormal, V
			, worldTangent, worldBiNormal, worldNormal
			, ray.direction, weight); // the new sampling direction will be in ray.direction

		if (ok == false)
		{
			break;
		}

		ray.origin = worldPosition;

		throughput *= weight;
	}
	return radiance;
}

vec3 rasterizationEmulatedByRayTrace(const ivec2 imageCoords, const ivec2 imageSize)
{
	Ray ray = calculateRay(imageCoords, imageSize);

	vec3 hitValue = vec3(0.0);
	vec3 weight = vec3(0.0);

	payLoad.hitT = INFINITY;
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, ray.origin, T_MIN, ray.direction, T_MAX, 0);

	if (payLoad.hitT == INFINITY)
	{
		vec3 sampleCoords = normalize(ray.direction);
		sampleCoords.xy *= pushConstants.environmentMapCoordTransform.xy;
		hitValue = texture(environmentMap, sampleCoords).xyz;
		return hitValue;
	}

	const Model model = models._models[payLoad.instanceCustomIndex];

	vec3 geometryNormal;
	const Vertex vertex = loadVertex(model, geometryNormal);
	const MaterialProperties materialProperties = loadMaterialProperties(model, vertex);

	// early out
	if (pushConstants.materialComponentViz > 0)
	{
		return materialComponentViz(materialProperties);
	}

	Ray currentRay = ray;
	
	vec3 normalViewSpace = (transpose(inverse(sceneUbo.viewMatrix)) * vec4(vertex.normal, 1)).xyz;
	vec3 vertexViewSpace = (sceneUbo.viewMatrix * vec4(vertex.position, 1.0)).xyz;

	vec3 lit = vec3(dot(normalize(normalViewSpace), normalize(-vertexViewSpace)));

	vec3 final = materialProperties.baseColor * lit;
	if (pushConstants.reflectivity > 0)
	{
		vec3 incidentVector = normalize(vertexViewSpace);
		vec3 reflectedVector = reflect(incidentVector, normalViewSpace);
		vec3 reflectedVectorWorldSpace = normalize(vec3(sceneUbo.viewMatrixInverse * vec4(reflectedVector, 0)));

		// use the lod to sample an lod to simulate roughness
		float maxLod = floor(log2(textureSize(environmentMap, 0))).x;
		float lod = pushConstants.textureLodBias * maxLod;
		vec3 reflectedColor = textureLod(environmentMap, reflectedVectorWorldSpace.xyz * vec3(pushConstants.environmentMapCoordTransform.xy, 1), lod).xyz;
		final = mix(final, reflectedColor, pushConstants.reflectivity);
	}
	hitValue = final;

	return hitValue;
}