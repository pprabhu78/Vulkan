// Copyright 2020 Google LLC

struct VSInput
{
[[vk::location(0)]] float3 Pos : POSITION0;
[[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct UBO
{
	float4x4 projection;
	float4x4 view;
	float4 instances[8];
};

cbuffer ubo : register(b0) { UBO ubo; }

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float3 UV : TEXCOORD0;
};

VSOutput main(VSInput input, uint InstanceIndex : SV_InstanceID)
{
	VSOutput output = (VSOutput)0;
	output.UV = float3(input.UV, ubo.instances[InstanceIndex].w);
	float3 position = input.Pos + ubo.instances[InstanceIndex].xyz;
	output.Pos = mul(ubo.projection, mul(ubo.view, float4(position, 1.0)));
	return output;
}
