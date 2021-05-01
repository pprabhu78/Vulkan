// Copyright 2020 Sascha Willems

struct VSOutput
{
	float4 Pos : SV_POSITION;
};

struct Node
{
    float4 color;
    float depth;
    uint next;
};

struct GeometrySBO
{
    uint count;
    uint maxNodeCount;
};

RWTexture2D<uint> headIndexImage : register(u0, space1);

RWStructuredBuffer<Node> nodes : register(u1, space1);

RWStructuredBuffer<GeometrySBO> geometrySBO : register(u2, space1);

struct PushConsts {
	float4x4 model;
	float4 color;
};
[[vk::push_constant]] PushConsts pushConsts;

[earlydepthstencil]
void main(VSOutput input)
{
    // Increase the node count
    uint nodeIdx;
    InterlockedAdd(geometrySBO[0].count, 1, nodeIdx);

    // Check LinkedListSBO is full
    if (nodeIdx < geometrySBO[0].maxNodeCount)
    {
        // Exchange new head index and previous head index
        uint prevHeadIdx;
        InterlockedExchange(headIndexImage[uint2(input.Pos.xy)], nodeIdx, prevHeadIdx);

        // Store node data
        nodes[nodeIdx].color = pushConsts.color;
        nodes[nodeIdx].depth = input.Pos.z;
        nodes[nodeIdx].next = prevHeadIdx;
    }
}