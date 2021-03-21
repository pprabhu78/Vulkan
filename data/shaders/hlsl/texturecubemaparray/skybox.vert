// Copyright 2020 Google LLC

struct UBO
{
	float4x4 projection;
	float4x4 model;
	float4x4 invModel;
	float lodBias;
	int cubeMapIndex;
};

cbuffer ubo : register(b0) { UBO ubo; }

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float3 UVW : TEXCOORD0;
};

VSOutput main([[vk::location(0)]] float3 Pos : POSITION0)
{
	VSOutput output = (VSOutput)0;
	output.UVW = Pos;
	output.UVW.yz *= -1.0;
	// Cancel out the translation part of the modelview matrix, as the skybox needs to stay centered
	float4x4 modelCentered = ubo.model;
	modelCentered[0][3] = 0.0;
	modelCentered[1][3] = 0.0;
	modelCentered[2][3] = 0.0;
	modelCentered[3][3] = 1.0;
	output.Pos = mul(ubo.projection, mul(modelCentered, float4(Pos.xyz, 1.0)));
	return output;
}
