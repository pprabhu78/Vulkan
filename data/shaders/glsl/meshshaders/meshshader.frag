#version 450

layout(location = 0) out vec4 outFragColor;

layout(location=0) in Interpolants {
	vec3 V;
	vec3 N;
	flat uint meshletIndex;
} IN;

uint hash( uint x ) 
{
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

void main()
{
	float val = dot(IN.V, IN.N);

    uint random = hash(IN.meshletIndex);
    float red = float((random % 256)) / 256.0;
    float green = float(((random >> 8) % 256)) / 256.0;
    float blue = float(((random >> 16) % 256)) / 256.0;

	outFragColor = vec4(red*val,green*val,blue*val,1);
}