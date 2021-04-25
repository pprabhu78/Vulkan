// Copyright 2020 Google LLC

Texture2D textureColorMap : register(t0, space1);
SamplerState samplerColorMap : register(s0, space1);
Texture2D textureNormalHeightMap : register(t1, space1);
SamplerState samplerNormalHeightMap : register(s1, space1);

#define lightRadius 45.0

struct VSOutput
{
[[vk::location(0)]] float2 UV : TEXCOORD0;
[[vk::location(1)]] float3 LightVec : TEXCOORD2;
[[vk::location(2)]] float3 LightVecB : TEXCOORD3;
[[vk::location(3)]] float3 LightDir : TEXCOORD4;
[[vk::location(4)]] float3 ViewVec : TEXCOORD1;
};

float4 main(VSOutput input) : SV_TARGET
{
	float3 specularColor = float3(0.85, 0.5, 0.0);

	float3 rgb = textureColorMap.Sample(samplerColorMap, input.UV).rgb;
	float3 normal = normalize((textureNormalHeightMap.Sample(samplerNormalHeightMap, input.UV).rgb - 0.5) * 2.0);

	float distSqr = dot(input.LightVecB, input.LightVecB);
	float3 lVec = input.LightVecB * rsqrt(distSqr);

	float diffuse = clamp(dot(lVec, normal), 0.0, 1.0);

	float3 light = normalize(-input.LightVec);
	float3 view = normalize(input.ViewVec);
	float3 reflectDir = reflect(-light, normal);

	float specular = pow(max(dot(view, reflectDir), 0.0), 4.0);

	return float4((rgb + (diffuse * rgb + 0.5 * specular * specularColor.rgb)), 1.0);
}