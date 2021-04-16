#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (set = 0, binding = 0) uniform UBO
{
	mat4 projection;
	mat4 model;
} ubo;

layout (location = 0) out vec3 outColor;

void main() 
{
	outColor = inColor;
	gl_Position = ubo.projection * ubo.model * vec4(inPos, 1.0);
}
