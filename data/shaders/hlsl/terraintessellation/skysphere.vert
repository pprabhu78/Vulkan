// Copyright 2020 Google LLC

struct VSInput
{
[[vk::location(0)]] float3 Pos : POSITION0;
[[vk::location(1)]] float3 Normal : NORMAL0;
[[vk::location(2)]] float2 UV : TEXCOORD0;
};

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float2 UV : TEXCOORD0;
};

struct UBO
{
	float4x4 projection;
	float4x4 modelview;
	float4 lightPos;
	float4 frustumPlanes[6];
	float displacementFactor;
	float tessellationFactor;
	float2 viewportDim;
	float tessellatedEdgeSize;
};
cbuffer ubo : register(b0) { UBO ubo; };

VSOutput main(VSInput input)
{
	VSOutput output = (VSOutput)0;
	// Cancel out the translation part of the modelview matrix, as the skybox needs to stay centered
	float4x4 modelCentered = ubo.modelview;
	modelCentered[0][3] = 0.0;
	modelCentered[1][3] = 0.0;
	modelCentered[2][3] = 0.0;
	modelCentered[3][3] = 1.0;
	output.Pos = mul(ubo.projection, mul(modelCentered, float4(input.Pos.xyz, 1.0)));
	output.UV = input.UV;
	return output;
}
