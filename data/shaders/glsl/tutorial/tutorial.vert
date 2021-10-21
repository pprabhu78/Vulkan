#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;



layout (binding = 0) uniform UBO 
{
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
} shaderUbo;



layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outColor;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	outColor = inColor;
	outUV = inUV;
	
	gl_Position = shaderUbo.projectionMatrix * shaderUbo.modelViewMatrix * vec4(inPosition, 1.0);
}
