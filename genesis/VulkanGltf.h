#pragma once

#include <string>
#include <vector>
#include <functional>

#include "GenMath.h"

#include <vulkan/vulkan.h>

#define CPU_SIDE_COMPILATION 1
#include "../data/shaders/glsl/common/gltfMaterial.h"

namespace tinygltf
{
   class Model;
   class Node;
   struct Mesh;
}

namespace genesis
{
   class Device;
   class Image;
   class Texture;
   class Buffer;
   class AccelerationStructure;

   struct Vertex
   {
      Vector3_32 position;
      Vector3_32 normal;
      Vector2_32 uv;
      Vector4_32 color;
   };

   struct Primitive
   {
      uint32_t firstIndex;
      uint32_t indexCount;

      uint32_t firstVertex;
      uint32_t vertexCount;

      int32_t materialIndex;
   };

   class Mesh 
   {
   public:
      std::vector<Primitive> primitives;
   };

   struct Node
   {
   public:
      virtual Matrix4_32 fullTransform() const;
   public:
      Node* _parent;
      std::vector<Node*> _children;
      Mesh* _mesh;

      Matrix4_32 _matrix;
      glm::vec3 _translation{};
      glm::quat _rotation{};
      glm::vec3 _scale{ 1.0f };
   };

   enum LightType
   {
        POINT = 0
      , SPOT
      , DIRECTIONAL
   };

   struct Light
   {
   public:
      LightType _lightType;
      Vector3_32 _color;
      float _intensity;
   };

   struct LightInstance
   {
      Light _light;
      Vector3_32 _position;
   };

   class VulkanGltfModel
   {
   public:
      enum FileLoadingFlags {
         None = 0x00000000,
         PreTransformVertices = 0x00000001,
         PreMultiplyVertexColors = 0x00000002,
         FlipY = 0x00000004,
         DontLoadImages = 0x00000008
      };
   public:
      VulkanGltfModel(Device* device, bool rayTracing);
      virtual ~VulkanGltfModel();
   public:
      virtual void loadFromFile(const std::string& fileName, uint32_t fileLoadingFlags);

      virtual const Buffer* vertexBuffer(void) const;
      virtual const Buffer* indexBuffer(void) const;
      virtual int numVertices() const;

      virtual const std::vector<Node*>& linearNodes(void) const;
      virtual const std::vector<Texture*>& textures(void) const;
      virtual const std::vector<Material>& materials(void) const;

      virtual void forEachPrimitive(const std::function<void(const Primitive&)>& func) const;
      virtual int numPrimitives(void) const;
   protected:
      virtual void loadImages(tinygltf::Model& gltfModel);
      virtual void loadTextures(tinygltf::Model& gltfModel);
      virtual void loadMaterials(tinygltf::Model& gltfModel);
      virtual void loadScenes(tinygltf::Model& gltfModel);
      virtual void loadNode(const tinygltf::Node& inputNode, tinygltf::Model& gltfModel, Node* parent);
      virtual void loadMesh(Node* node, const tinygltf::Mesh& srcMesh, tinygltf::Model& gltfModel);
      virtual void loadLights(tinygltf::Model& gltfModel);

      virtual const std::vector<Image*>& images(void) const;

      virtual void bakeAttributes(tinygltf::Model& gltfModel, uint32_t fileLoadingFlags);

      virtual void buildLightInstancesBuffer(void);
   protected:
      Device* _device;

      std::vector<Image*> _images;

      std::vector<Texture*> _textures;
      
      std::vector<Material> _materials;

      std::vector<Node*> _linearNodes;

      std::string _basePath;

      std::vector<uint32_t> _indexBuffer;
      std::vector<Vertex> _vertexBuffer;

      Buffer* _vertexBufferGpu;
      Buffer* _indexBufferGpu;

      // original lights
      std::vector<Light*> _lights;

      // instance light (has transformation)
      std::vector<LightInstance> _lightInstances;

      // same as above, but on the gpu
      Buffer* _lightInstancesGpu;

      const bool _rayTracing;

      static const int s_maxBindlessTextures;
   };
}