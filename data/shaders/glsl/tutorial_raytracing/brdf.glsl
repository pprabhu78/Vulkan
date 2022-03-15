#include "sampling.glsl"


// BRDF types
#define DIFFUSE_TYPE 1
#define SPECULAR_TYPE 2

#define UNIFORM_SAMPLING 0
#define COSINE_SAMPLING 1

struct MaterialProperties
{
   vec3 baseColor;
   float metalness;
   float roughness;
};

vec3 sampleHemisphere(float samplingType, vec2 u, out float pdf)
{
   vec3 newRayDirection;
   if (samplingType == COSINE_SAMPLING)
   {
      newRayDirection = cosineSampleHemisphere(u, pdf);
   }
   else if (samplingType == UNIFORM_SAMPLING)
   {
      newRayDirection = uniformSampleHemisphere(u, pdf);
   }
   else
   {
      newRayDirection = vec3(0, 0, 1);
   }
   return newRayDirection;
}

vec3 diffuseBrdfWeight(int samplingType, MaterialProperties material
, vec3 worldNormal, vec3 newRayDirection)
{
   vec3 weight;
   if (samplingType == COSINE_SAMPLING)
   {
      // Crash Course in BRDF Implementation
      // https://boksajak.github.io/blog/BRDF
      // 3.1 Lambertian diffuse BRDF
      //   (brdf * cosTheta)/pdf
      // ->((decal/pi) * cosTheta)/ (cosTheta/Pi))
      // -> decal
      weight = material.baseColor;
   }
   else if (samplingType == UNIFORM_SAMPLING)
   {
      //   (brdf * cosTheta)/pdf
      // ->((decal/pi) * cosTheta)/ (1/(2*Pi))
      // -> decal * cosTheta * 2
      // Multiplication by 2 is correct. See here:
      // https://graphicscompendium.com/raytracing/19-monte-carlo
      float cosTheta = dot(newRayDirection, worldNormal);
      weight = material.baseColor * cosTheta * 2.0f;
   }
   else
   {
      weight = vec3(0, 0, 0);
   }
   return weight;
}

bool evaluateBrdf(int brdfType, int samplingType, vec2 u
   , MaterialProperties material, vec3 worldNormal
   , vec3 T, vec3 B, vec3 N
   , out vec3 newRayDirection, out vec3 weight)
{
   if (brdfType == DIFFUSE_TYPE)
   {
      float pdf = 0;
      newRayDirection = sampleHemisphere(samplingType, u, pdf);

      // xform to the same space as the world normal
      newRayDirection = newRayDirection.x * T + newRayDirection.y * B + newRayDirection.z * N;

      weight = diffuseBrdfWeight(samplingType, material, worldNormal, newRayDirection);
   }
   else
   {
      weight = vec3(0, 0, 0);
   }

   return true;
}