#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2) uniform UBO 
{
	mat4 viewMatrix;
	mat4 viewInverse;
	mat4 projInverse;
	int vertexSizeInBytes;
} sceneUbo;

layout(set = 0, binding = 3) buffer Vertices { vec4 v[]; } vertices;
layout(set = 0, binding = 4) buffer Indices { uint i[]; } indices;

struct Material
{
	vec4 baseColorFactor;
	vec3 padding;
    uint baseColorTextureIndex;
};


layout (set = 1, binding = 0, std430) readonly buffer materialBuffer
{
	Material _materialBuffer[];
};

layout (set = 1, binding = 1, std430) readonly buffer materialIndices
{
	uint _materialIndices[];
};
layout (set = 1, binding = 2, std430) readonly buffer indexIndices
{
	uint _indexIndices[];
};

layout (set = 1, binding = 3) uniform sampler2D samplers[];

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

struct Vertex
{
  vec3 pos;
  vec3 normal;
  vec2 uv;
  vec4 color;
};

Vertex unpack(uint index, int vertexSizeInBytes)
{
	// Unpack the vertices from the SSBO using the glTF vertex structure
	// The multiplier is the size of the vertex divided by four float components (=16 bytes)
	const int m =  vertexSizeInBytes/ 16;

	vec4 d0 = vertices.v[m * index + 0];
	vec4 d1 = vertices.v[m * index + 1];
	vec4 d2 = vertices.v[m * index + 2];

	Vertex v;
	v.pos = d0.xyz;
	v.normal = vec3(d0.w, d1.x, d1.y);
	v.uv = vec2(d1.z, d1.w);
	v.color = vec4(d2.x, d2.y, d2.z, 1.0);
	return v;

	return v;
}

void main()
{
    uint indicesOffset = _indexIndices[gl_GeometryIndexEXT];
	ivec3 index = ivec3(indices.i[indicesOffset + (3 * gl_PrimitiveID)], indices.i[ indicesOffset + (3 * gl_PrimitiveID + 1)], indices.i[ indicesOffset + (3 * gl_PrimitiveID + 2)]);

	Vertex v0 = unpack(index.x, sceneUbo.vertexSizeInBytes);
	Vertex v1 = unpack(index.y, sceneUbo.vertexSizeInBytes);
	Vertex v2 = unpack(index.z, sceneUbo.vertexSizeInBytes);

	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

	const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	const vec3 position = v0.pos * barycentricCoords.x + v1.pos * barycentricCoords.y + v2.pos * barycentricCoords.z;
	const vec2 uv = v0.uv * barycentricCoords.x + v1.uv * barycentricCoords.y + v2.uv * barycentricCoords.z;
	const vec4 color = v0.color.x * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z;

	vec3 normalViewSpace = (transpose(inverse(sceneUbo.viewMatrix))*vec4(normal.x,normal.y, normal.z, 1)).xyz;
	vec3 vertexViewSpace = (sceneUbo.viewMatrix * vec4(position, 1.0)).xyz;

	uint samplerIndex = _materialBuffer[_materialIndices[gl_GeometryIndexEXT]].baseColorTextureIndex;

	vec3 decal = texture(samplers[samplerIndex], uv).rgb*color.rgb;

	vec3 lit = vec3(dot(normalize(normalViewSpace), normalize(-vertexViewSpace)));

	hitValue = decal*lit;
}
