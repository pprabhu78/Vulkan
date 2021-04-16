// Copyright 2020 Google LLC

Texture2D textureColor : register(t0, space1);
SamplerState samplerColor : register(s0, space1);

float4 main([[vk::location(0)]] float2 inUV : TEXCOORD0) : SV_TARGET
{
	return textureColor.Sample(samplerColor, inUV);
}