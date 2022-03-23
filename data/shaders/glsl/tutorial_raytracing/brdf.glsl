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


float3 diffuseReflectance(MaterialProperties material)
{
   return material.baseColor * (1.0f - material.metalness);
}

vec3 diffuseBrdfWeight(int samplingType, MaterialProperties material
, vec3 N, vec3 L)
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
      weight = diffuseReflectance(material);
   }
   else if (samplingType == UNIFORM_SAMPLING)
   {
      //   (brdf * cosTheta)/pdf
      // ->((decal/pi) * cosTheta)/ (1/(2*Pi))
      // -> decal * cosTheta * 2
      // Multiplication by 2 is correct. See here:
      // https://graphicscompendium.com/raytracing/19-monte-carlo
      float cosTheta = dot(N, L);
      weight = diffuseReflectance(material) * cosTheta * 2.0f;
   }
   else
   {
      weight = vec3(0, 0, 0);
   }
   return weight;
}

// Crash Course in BRDF Implementation
// https://boksajak.github.io/blog/BRDF, section 4.2
// substitute lambda, then substitute 'a'
float Smith_G1_GGX(float alpha, float alphaSquared, float NdotS, float NdotSSquared)
{
   return 2.0f / (sqrt(((alphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
}

// Implementing a Simple Anisotropic Rough Diffuse Material with Stochastic Evaluation
// Eric Heitz and Jonathan Dupy
// page 6/appendix A, (9)
float Smith_G2_Over_G1_Height_Correlated(float alpha, float alphaSquared, float NdotL, float NdotV) 
{
   float G1V = Smith_G1_GGX(alpha, alphaSquared, NdotV, NdotV * NdotV);
   float G1L = Smith_G1_GGX(alpha, alphaSquared, NdotL, NdotL * NdotL);
   return G1L / (G1V + G1L - G1V * G1L);
}

// https://en.wikipedia.org/wiki/Schlick%27s_approximation, quote: 
// In microfacet models it is assumed that there is always a perfect reflection, 
// but the normal changes according to a certain distribution, resulting in a non-perfect overall reflection. 
// When using Schlick’s approximation, the normal in the above computation is replaced by the halfway vector. 
// Either the viewing or light direction can be used as the second vector.
vec3 fresnelSchlick(vec3 f0, float f90, float NdotV)
{
   return f0 + (f90 - f0) * pow(1.0f - NdotV, 5.0f);
}

// Samples a microfacet normal for the GGX distribution using VNDF method.
// Source: "Sampling the GGX Distribution of Visible Normals" by Heitz
// See also https://hal.inria.fr/hal-00996995v1/document and http://jcgt.org/published/0007/04/01/
// Random variables 'u' must be in <0;1) interval
// PDF is 'G1(NdotV) * D'
// Ve: view direction
// Output: direction sampled with PDF: (G1(Ve)*D(Ne)*max(0, dot(Ve, Ne)))/Ve.z;
float3 sampleGGXVNDF(float3 Ve, float2 alpha2D, float2 u) 
{
   // Section 3.2: transforming the view direction to the hemisphere configuration
   float3 Vh = normalize(float3(alpha2D.x * Ve.x, alpha2D.y * Ve.y, Ve.z));

   // Section 4.1: orthonormal basis (with special case if cross product is zero)
   float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
   float3 T1 = lensq > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) * rsqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
   float3 T2 = cross(Vh, T1);

   // Section 4.2: parameterization of the projected area
   float r = sqrt(u.x);
   float phi = TWO_PI * u.y;
   float t1 = r * cos(phi);
   float t2 = r * sin(phi);
   float s = 0.5f * (1.0f + Vh.z);
   t2 = lerp(sqrt(1.0f - t1 * t1), t2, s);

   // Section 4.3: reprojection onto hemisphere
   float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

   // Section 3.4: transforming the normal back to the ellipsoid configuration
   return normalize(float3(alpha2D.x * Nh.x, alpha2D.y * Nh.y, max(0.0f, Nh.z)));
}

vec3 calculateSpecularF0(MaterialProperties material)
{
   return mix(vec3(F0_DIELECTRICS, F0_DIELECTRICS, F0_DIELECTRICS)
      , material.baseColor, material.metalness);
}

float calculateSpecularF90(vec3 specularF0)
{
   // This scalar value is somewhat arbitrary, Schuler used 60 in his article. In here, we derive it from MIN_DIELECTRICS_F0 so
   // that it takes effect for any reflectance lower than least reflective dielectrics
   //const float t = 60.0f;
   const float t = (1.0f / F0_DIELECTRICS);
   float specularF90 = min(1.0f, t * luminance(specularF0));

   return specularF90;
}

vec3 specularBrdfWeightAndDirection(int samplingType, MaterialProperties material
   , vec3 N, vec3 V, vec2 u, out vec3 newRayDirection)
{
   // alpha is roughness squared
   // https://boksajak.github.io/blog/BRDF, section 3.4
   float alpha = material.roughness * material.roughness;
   float alphaSquared = alpha * alpha;

   // get the rotation quaternian that takes this shading normal to (0,0,1) 
   float4 qRotationToZ = getRotationToZAxis(N);
   // transform the viewing direction (point) by this rotation to get into the same space
   float3 Vlocal = rotatePoint(qRotationToZ, V);

   // Sample a microfacet normal (H) in local space
   float3 Hlocal;
   if (alpha == 0.0f) 
   {
      // Fast path for zero roughness (perfect reflection), also prevents NaNs appearing due to divisions by zeroes
      Hlocal = float3(0.0f, 0.0f, 1.0f);
   }
   else 
   {
      // For non-zero roughness, this calls VNDF sampling for GG-X distribution or Walter's sampling for Beckmann distribution
      Hlocal = sampleGGXVNDF(Vlocal, float2(alpha, alpha), u);
   }

   // Reflect view direction to obtain light vector
   float3 Llocal = reflect(-Vlocal, Hlocal);
   newRayDirection = normalize(rotatePoint(invertRotation(qRotationToZ), Llocal));

   // Note: HdotL is same as HdotV here
   // Clamp dot products here to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
   float HdotL = max(0.00001f, min(1.0f, dot(Hlocal, Llocal)));
   const float3 Nlocal = float3(0.0f, 0.0f, 1.0f);
   float NdotL = max(0.00001f, min(1.0f, dot(Nlocal, Llocal)));
   float NdotV = max(0.00001f, min(1.0f, dot(Nlocal, Vlocal)));

   // https://boksajak.github.io/blog/BRDF, section 4.4
   // weight for vndf sampling is F*(G2/G1(H,V))

   // calculate g2/gl, height correlated
   float G2_over_G1 = Smith_G2_Over_G1_Height_Correlated(alpha, alphaSquared, NdotL, NdotV);

   vec3 specularF0 = calculateSpecularF0(material);
   float specularF90 = calculateSpecularF90(specularF0);

   // https://en.wikipedia.org/wiki/Schlick%27s_approximation
   // In microfacet models it is assumed that there is always a perfect reflection, 
   // but the normal changes according to a certain distribution, resulting in a non-perfect overall reflection. 
   // When using Schlick’s approximation, the normal in the above computation is replaced by the halfway vector. 
   // Either the viewing or light direction can be used as the second vector.
   // so, NdoV is actually HdotV and since HdotL is same as HdotV, we pass in HdotL
   vec3 F = fresnelSchlick(specularF0, specularF90, HdotL);

   vec3 weight = F * G2_over_G1;

   return weight;
}

// Calculates probability of selecting BRDF (specular or diffuse) using the approximate Fresnel term
float getBrdfProbability(MaterialProperties material, float3 V, float3 shadingNormal) {

   // Evaluate Fresnel term using the shading normal
   // Note: we use the shading normal instead of the microfacet normal (half-vector) for Fresnel term here. That's suboptimal for rough surfaces at grazing angles, but half-vector is yet unknown at this point
   float specularF0 = luminance(calculateSpecularF0(material));
   float NdotV = max(0.0f, dot(V, shadingNormal));
   float fresnel = saturate(luminance(fresnelSchlick(vec3(specularF0), calculateSpecularF90(vec3(specularF0)), NdotV)));

   // Approximate relative contribution of BRDFs using the Fresnel term
   float specular = fresnel;

   float diffuseReflectance = luminance(diffuseReflectance(material));
   float diffuse = diffuseReflectance * (1.0f - fresnel); //< If diffuse term is weighted by Fresnel, apply it here as well

   // Return probability of selecting specular BRDF over diffuse BRDF
   float p = (specular / max(0.0001f, (specular + diffuse)));

   // Clamp probability to avoid undersampling of less prominent BRDF
   return clamp(p, 0.1f, 0.9f);
}


bool evaluateBrdf(int brdfType, int samplingType, vec2 u
   , MaterialProperties material, vec3 shadingNormal, vec3 V
   , vec3 T, vec3 B, vec3 N
   , out vec3 newRayDirection, out vec3 weight)
{
   if (brdfType == DIFFUSE_TYPE)
   {
      float pdf = 0;
      newRayDirection = sampleHemisphere(samplingType, u, pdf);

      // xform to the same space as the world normal
      newRayDirection = newRayDirection.x * T + newRayDirection.y * B + newRayDirection.z * N;

      weight = diffuseBrdfWeight(samplingType, material
         , shadingNormal, newRayDirection);
   }
   else if (brdfType == SPECULAR_TYPE)
   {
      // This calculates the new ray direction and weight in tandem
      weight = specularBrdfWeightAndDirection(samplingType, material
         , shadingNormal, V, u, newRayDirection);
   }
   else
   {
      weight = vec3(0, 0, 0);
      newRayDirection = vec3(0, 0, 0);
   }

   return true;
}