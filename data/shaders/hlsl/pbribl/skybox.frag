// Copyright 2020 Google LLC

TextureCube textureEnv : register(t0, space1);
SamplerState samplerEnv : register(s0, space1);

struct UBOParams {
	float4x4 projection;
	float4x4 model;
	float4x4 view;
	float4 camPos;
	float4 lights[4];
	float exposure;
	float gamma;
};
cbuffer uboParams : register(b0) { UBOParams uboParams; };

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 Uncharted2Tonemap(float3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

float4 main([[vk::location(0)]] float3 inUVW : POSITION0) : SV_TARGET
{
	float3 color = textureEnv.Sample(samplerEnv, inUVW).rgb;

	// Tone mapping
	color = Uncharted2Tonemap(color * uboParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap((11.2f).xxx));
	// Gamma correction
	color = pow(color, (1.0f / uboParams.gamma).xxx);

	return float4(color, 1.0);
}