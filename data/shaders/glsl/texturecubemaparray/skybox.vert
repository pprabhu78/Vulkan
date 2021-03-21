#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 invModel;
	float lodBias;
	int cubeMapIndex;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	outUVW.yz *= -1.0f;
	// Cancel out the translation part of the modelview matrix, as the skybox needs to stay centered
	mat4 modelCentered = mat4(mat3(ubo.model));
	gl_Position = ubo.projection * modelCentered * vec4(inPos.xyz, 1.0);
}
