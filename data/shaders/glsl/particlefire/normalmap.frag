#version 450

layout (set = 1, binding = 0) uniform sampler2D sColorMap;
layout (set = 1, binding = 1) uniform sampler2D sNormalHeightMap;

#define lightRadius 45.0

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inLightVec;
layout (location = 2) in vec3 inLightVecB;
layout (location = 3) in vec3 inLightDir;
layout (location = 4) in vec3 inViewVec;

layout (location = 0) out vec4 outFragColor;

void main(void) 
{
	vec3 specularColor = vec3(0.85, 0.5, 0.0);
	vec3 color = texture(sColorMap, inUV).rgb;
	vec3 normal = normalize((texture(sNormalHeightMap, inUV).rgb - 0.5) * 2.0);

	float distSqr = dot(inLightVecB, inLightVecB);
	vec3 lVec = inLightVecB * inversesqrt(distSqr);

	float diffuse = clamp(dot(lVec, normal), 0.0, 1.0);

	vec3 light = normalize(-inLightVec);
	vec3 view = normalize(inViewVec);
	vec3 reflectDir = reflect(-light, normal);
		
	float specular = pow(max(dot(view, reflectDir), 0.0), 4.0);
	
	outFragColor = vec4((color + (diffuse * color + 0.5 * specular * specularColor.rgb)), 1.0);   
}