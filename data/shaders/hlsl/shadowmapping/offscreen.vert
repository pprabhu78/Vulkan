// Copyright 2020 Google LLC

struct UBO
{
	float4x4 projection;
	float4x4 view;
	float4x4 model;
	float4x4 depthMVP;
	float4 lightPos;
};

cbuffer ubo : register(b0) { UBO ubo; }

float4 main([[vk::location(0)]] float3 Pos : POSITION0) : SV_POSITION
{
	return mul(ubo.depthMVP, float4(Pos, 1.0));
}