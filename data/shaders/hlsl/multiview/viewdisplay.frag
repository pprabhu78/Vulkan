// Copyright 2020 Google LLC
// Copyright 2021 Sascha Willems

Texture2DArray textureView : register(t0, space1);
SamplerState samplerView : register(s0, space1);

struct VSInput
{
[[vk::location(0)]] float2 UV : TEXCOORD0;
[[vk::location(1)]] uint LayerIndex : TEXCOOR1;
};

struct UBO
{
	[[vk::offset(272)]] float distortionAlpha;
};

cbuffer ubo : register(b0, space0) { UBO ubo; }

float4 main(VSInput input) : SV_TARGET
{
	const float alpha = ubo.distortionAlpha;

	float2 p1 = float2(2.0 * input.UV - 1.0);
	float2 p2 = p1 / (1.0 - alpha * length(p1));
	p2 = (p2 + 1.0) * 0.5;

	bool inside = ((p2.x >= 0.0) && (p2.x <= 1.0) && (p2.y >= 0.0 ) && (p2.y <= 1.0));
	return inside ? textureView.Sample(samplerView, float3(p2, input.LayerIndex)) : float4(0.0, 0.0, 0.0, 0.0);
}