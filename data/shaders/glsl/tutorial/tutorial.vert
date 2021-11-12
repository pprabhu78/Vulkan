#version 450


#extension GL_ARB_shader_draw_parameters : require

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
} sceneUbo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormalViewSpace;
layout (location = 3) out vec3 outVertexViewSpace;
layout (location = 4) flat out uint outDrawIndex;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	outColor = inColor;
	outUV = inUV;

	outNormalViewSpace = (transpose(inverse(sceneUbo.viewMatrix))*vec4(inNormal.x,inNormal.y, inNormal.z, 1)).xyz;
	
	outVertexViewSpace = (sceneUbo.viewMatrix * vec4(inPosition, 1.0)).xyz;
	
	gl_Position = sceneUbo.projectionMatrix * sceneUbo.viewMatrix * vec4(inPosition, 1.0);

	outDrawIndex = gl_DrawIDARB;
}
