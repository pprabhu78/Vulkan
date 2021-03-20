#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	vec4 instances[8];
} ubo;

layout (location = 0) out vec3 outUV;

void main() 
{
	outUV = vec3(inUV, ubo.instances[gl_InstanceIndex].w);
	vec3 position = inPos + ubo.instances[gl_InstanceIndex].xyz;
	gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
}
