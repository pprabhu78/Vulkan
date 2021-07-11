#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	mat4 depthMVP;
	vec4 lightPos;
} ubo;

void main()
{
	gl_Position =  ubo.depthMVP * vec4(inPos, 1.0);
}