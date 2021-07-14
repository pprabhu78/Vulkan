#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outPos;

void main() 
{
	outUV = inUV;
	outPos = ubo.projection * ubo.view * pushConsts.model * vec4(inPos, 1.0);
	gl_Position = outPos;		
}
