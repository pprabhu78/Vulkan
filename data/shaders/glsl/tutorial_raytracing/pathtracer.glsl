
layout(location = 0) rayPayloadEXT HitPayload payLoad;

vec3 pathTrace()
{
   vec3 radiance = vec3(0.0);
   return radiance;
}

vec3 samplePixel(const ivec2 imageCoords, const ivec2 imageSize)
{
	// compute the sampling position
	const vec2 pixelCenter = imageCoords + vec2(0.5);
	const vec2 inUV = pixelCenter / imageSize;
	vec2 d = inUV * 2.0 - 1.0;

	// compute the ray origina and direction
	vec4 rayOrigin = sceneUbo.viewMatrixInverse * vec4(0, 0, 0, 1);
	vec4 target = sceneUbo.projectionMatrixInverse * vec4(d.x, d.y, 1, 1);
	vec4 rayDirection = sceneUbo.viewMatrixInverse * vec4(normalize(target.xyz), 0);

	const float tmin = 0.001;
	const float tmax = 10000.0;

	payLoad.hitValue = vec3(0.0);
	payLoad.rayOrigin = rayOrigin.xyz;
	payLoad.rayDirection = rayDirection.xyz;
	payLoad.depth = 0;
	payLoad.weight = vec3(0.0);

	vec3 throughput = vec3(1);
	vec3 radiance = vec3(0);

	for (; payLoad.depth < 10; payLoad.depth++)
	{
		traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, payLoad.rayOrigin, tmin, payLoad.rayDirection, tmax, 0);

		radiance += payLoad.hitValue * throughput;
		throughput *= payLoad.weight;
	}
	return radiance;
}

vec3 samplePixel2(const ivec2 imageCoords, const ivec2 imageSize)
{
	// compute the sampling position
	const vec2 pixelCenter = imageCoords + vec2(0.5);
	const vec2 inUV = pixelCenter / imageSize;
	vec2 d = inUV * 2.0 - 1.0;

	// compute the ray origina and direction
	vec4 rayOrigin = sceneUbo.viewMatrixInverse * vec4(0, 0, 0, 1);
	vec4 target = sceneUbo.projectionMatrixInverse * vec4(d.x, d.y, 1, 1);
	vec4 rayDirection = sceneUbo.viewMatrixInverse * vec4(normalize(target.xyz), 0);

	const float tmin = 0.001;
	const float tmax = 10000.0;

	payLoad.hitValue = vec3(0.0);
	payLoad.rayOrigin = rayOrigin.xyz;
	payLoad.rayDirection = rayDirection.xyz;
	payLoad.depth = 0;
	payLoad.weight = vec3(0.0);

	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, payLoad.rayOrigin, tmin, payLoad.rayDirection, tmax, 0);

	return vec3(0.0);
}