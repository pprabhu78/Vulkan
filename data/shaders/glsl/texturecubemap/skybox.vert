#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	// Convert cubemap coordinates into Vulkan coordinate space
	outUVW.xy *= -1.0;
	// Cancel out the translation part of the modelview matrix, as the skybox needs to stay centered
	mat4 modelCentered = mat4(mat3(ubo.model));
	gl_Position = ubo.projection * modelCentered * vec4(inPos.xyz, 1.0);
}
