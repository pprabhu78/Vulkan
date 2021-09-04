// Copyright 2020 Google LLC
// Copyright 2021 Sascha Willems

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float2 UV : TEXCOORD0;
[[vk::location(1)]] uint LayerIndex : TEXCOORD1;
};

VSOutput main(uint VertexIndex : SV_VertexID, uint InstanceIndex : SV_InstanceID)
{
	VSOutput output = (VSOutput)0;
	output.UV = float2((VertexIndex << 1) & 2, VertexIndex & 2);
	output.Pos = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
	output.LayerIndex = InstanceIndex;
	return output;
}
