#version 450

layout (binding = 1) uniform sampler2D samplerFont;

layout (binding = 2) uniform UBO 
{
	vec4 outlineColor;
	float outlineWidth;
	float outline;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() 
{
    const float pxRange = 3.0f;
    vec2 msdfUnit = pxRange/vec2(textureSize(samplerFont, 0));
    vec3 smpl = texture(samplerFont, inUV).rgb;
    float sigDist = median(smpl.r, smpl.g, smpl.b) - 0.5;
    sigDist *= dot(msdfUnit, 0.5/fwidth(inUV));
    float opacity = clamp(sigDist + 0.5, 0.0, 1.0);
    float smoothWidth = fwidth(sigDist);	
    float alpha = smoothstep(0.5 - smoothWidth, 0.5 + smoothWidth, sigDist);
    const vec3 bgColor = vec3(0.0f);
    const vec3 fgColor = vec3(1.0f);
    vec3 color = mix(bgColor, fgColor, opacity);
    outFragColor = vec4(color.rgb, alpha);
}