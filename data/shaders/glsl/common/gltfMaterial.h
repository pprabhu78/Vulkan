#if CPU_SIDE_COMPILATION
#pragma once
using namespace glm;
namespace genesis{
#endif
struct Material
{
   vec4 baseColorFactor;
   vec4 emissiveFactor;
   vec3 padding;
   uint baseColorTextureIndex;

#if CPU_SIDE_COMPILATION
   Material::Material()
      : baseColorFactor(vec4(1.0f))
      , emissiveFactor(vec4(0.0f))
      , padding(vec3(0, 0, 0))
      , baseColorTextureIndex(0)
   {
      // nothing else
}
#endif
};

#if CPU_SIDE_COMPILATION
}
#endif