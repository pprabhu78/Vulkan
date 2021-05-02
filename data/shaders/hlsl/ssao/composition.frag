// Copyright 2020 Google LLC

struct UBO
{
	float4x4 projection;
	float4x4 model;
	float4x4 view;
	float nearPlane;
	float farPlane;
	float _pad0;
	float _pad1;
	int ssao;
	int ssaoOnly;
	int ssaoBlur;
};
cbuffer uboParams : register(b0, space0) { UBO uboParams; };

Texture2D textureposition : register(t0, space1);
SamplerState samplerposition : register(s0, space1);
Texture2D textureNormal : register(t1, space1);
SamplerState samplerNormal : register(s1, space1);
Texture2D textureAlbedo : register(t2, space1);
SamplerState samplerAlbedo : register(s2, space1);
Texture2D textureSSAO : register(t3, space1);
SamplerState samplerSSAO : register(s3, space1);
Texture2D textureSSAOBlur : register(t4, space1);
SamplerState samplerSSAOBlur : register(s4, space1);

float4 main([[vk::location(0)]] float2 inUV : TEXCOORD0) : SV_TARGET
{
	float3 fragPos = textureposition.Sample(samplerposition, inUV).rgb;
	float3 normal = normalize(textureNormal.Sample(samplerNormal, inUV).rgb * 2.0 - 1.0);
	float4 albedo = textureAlbedo.Sample(samplerAlbedo, inUV);

	float ssao = (uboParams.ssaoBlur == 1) ? textureSSAOBlur.Sample(samplerSSAOBlur, inUV).r : textureSSAO.Sample(samplerSSAO, inUV).r;

	float3 lightPos = float3(0.0, 0.0, 0.0);
	float3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	float4 outFragColor;
	if (uboParams.ssaoOnly == 1)
	{
		outFragColor.rgb = ssao.rrr;
	}
	else
	{
		float3 baseColor = albedo.rgb * NdotL;

		if (uboParams.ssao == 1)
		{
			outFragColor.rgb = ssao.rrr;

			if (uboParams.ssaoOnly != 1)
				outFragColor.rgb *= baseColor;
		}
		else
		{
			outFragColor.rgb = baseColor;
		}
	}
	return outFragColor;
}