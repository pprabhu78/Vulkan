#pragma once


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace genesis
{
   typedef glm::vec4 Vector4_32;
   typedef glm::vec3 Vector3_32;
   typedef glm::vec2 Vector2_32;
   typedef glm::mat4 Matrix4_32;
   typedef glm::mat3 Matrix3_32;
}