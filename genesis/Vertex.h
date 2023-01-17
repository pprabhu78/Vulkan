#pragma once


#include "GenMath.h"

namespace genesis
{
   struct Vertex
   {
      Vector3_32 position;
      Vector3_32 normal;
      Vector2_32 uv;
      Vector4_32 color;
   };

   struct VertexPositionNormal
   {
      Vector3_32 position;
      Vector3_32 normal;
   };
}