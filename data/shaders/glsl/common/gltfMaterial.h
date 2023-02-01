#if CPU_SIDE_COMPILATION
#pragma once
using namespace glm;
namespace genesis{
#endif

#define ALPHA_OPAQUE 0
#define ALPHA_MASK 1
#define ALPHA_BLEND 2

struct Material
{
   vec4 baseColorFactor;
   vec3 emissiveFactor;
   int emissiveTextureIndex;

   vec3 padding;
   int baseColorTextureIndex;

   float roughness;
   float metalness;

   // When using an image, glTF expects the 
   // metallic values to be encoded in the blue(B) channel
   // and roughness to be encoded in the green(G) channel of the same image
   // https://docs.blender.org/manual/en/2.80/addons/io_scene_gltf2.html
   int occlusionRoughnessMetalnessTextureIndex;

   int normalTextureIndex;

   int  alphaMode;

   float transmissionFactor;
   int transmissionTextureIndex;

#if CPU_SIDE_COMPILATION
   Material::Material()
      : baseColorFactor(vec4(1.0f))
      , emissiveFactor(vec4(0.0f))
      , emissiveTextureIndex(-1)
      , padding(vec3(0, 0, 0))
      , baseColorTextureIndex(-1)
      , roughness(0)
      , metalness(0)
      , occlusionRoughnessMetalnessTextureIndex(-1)
      , normalTextureIndex(-1)
      , alphaMode(ALPHA_OPAQUE)
      , transmissionFactor(0)
      , transmissionTextureIndex(-1)
   {
   }
#endif
};

#if CPU_SIDE_COMPILATION
}
#endif