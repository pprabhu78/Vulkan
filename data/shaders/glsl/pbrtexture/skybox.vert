#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	// Cancel out the translation part of the modelview matrix, as the skybox needs to stay centered
	mat4 centeredMat = mat4(mat3(ubo.view));
	gl_Position = ubo.projection * centeredMat * vec4(inPos.xyz, 1.0);
}
