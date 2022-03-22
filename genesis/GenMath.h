#pragma once


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <iostream>

namespace genesis
{
   typedef glm::vec4 Vector4_32;
   typedef glm::vec3 Vector3_32;
   typedef glm::vec2 Vector2_32;
   typedef glm::mat4 Matrix4_32;
   typedef glm::mat3 Matrix3_32;

   inline std::ostream& operator <<(std::ostream& o, const glm::vec3& v)
   {
      o << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
      return o;
   }

   inline std::ostream& operator <<(std::ostream& o, const glm::vec4& v)
   {
      o << "Vector4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
      return o;
   }
}