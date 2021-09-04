#version 450

layout (set = 1, binding = 0) uniform sampler2DArray samplerView;

layout (set = 0, binding = 0) uniform UBO 
{
	layout(offset = 272) float distortionAlpha;
} ubo;

layout (location = 0) in vec2 inUV;
layout (location = 1) flat in uint inLayerIndex;

layout (location = 0) out vec4 outColor;

void main() 
{
	const float alpha = ubo.distortionAlpha;

	vec2 p1 = vec2(2.0 * inUV - 1.0);
	vec2 p2 = p1 / (1.0 - alpha * length(p1));
	p2 = (p2 + 1.0) * 0.5;

	bool inside = ((p2.x >= 0.0) && (p2.x <= 1.0) && (p2.y >= 0.0 ) && (p2.y <= 1.0));
	outColor = inside ? texture(samplerView, vec3(p2, float(inLayerIndex))) : vec4(0.0);
}