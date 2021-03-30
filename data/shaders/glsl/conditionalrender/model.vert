#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;

layout (set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 view;
	mat4 model;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	vec4 pos = vec4(inPos, 1.0);
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;

	outNormal = mat3(ubo.view * ubo.model) * inNormal;

	vec4 localpos = ubo.view * ubo.model * pos;
	vec3 lightPos = vec3(10.0f, 10.0f, 10.0f);
	outLightVec = lightPos.xyz - localpos.xyz;
	outViewVec = -localpos.xyz;		
}
