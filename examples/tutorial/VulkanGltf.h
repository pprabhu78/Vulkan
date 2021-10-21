#pragma once

#include <string>
#include <vector>

#include "GenMath.h"

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

   struct Material
   {
      Vector4_32 baseColorFactor = Vector4_32(1.0f);
      uint32_t baseColorTextureIndex;
   };

   struct Vertex
   {
      Vector3_32 pos;
      Vector3_32 normal;
      Vector2_32 uv;
      Vector3_32 color;
   };

   struct Primitive
   {
      uint32_t firstIndex;
      uint32_t indexCount;
      int32_t materialIndex;
   };


   struct Mesh 
   {
      std::vector<Primitive> primitives;
   };


   struct Node
   {
      Node* parent;
      std::vector<Node> children;
      Mesh mesh;
      glm::mat4 matrix;
   };

   class VulkanGltfModel
   {
   public:
      VulkanGltfModel(Device* device);
      virtual ~VulkanGltfModel();
   public:
      virtual void loadFromFile(const std::string& fileName);

      virtual const std::vector<Image*>& textures(void) const;
      virtual const Buffer* vertexBuffer(void) const;
      virtual const Buffer* indexBuffer(void) const;

   protected:
      virtual void loadImages(tinygltf::Model& gltfModel);
      virtual void loadTextures(tinygltf::Model& gltfModel);
      virtual void loadMaterials(tinygltf::Model& gltfModel);
      virtual void loadScenes(tinygltf::Model& gltfModel);
      virtual void loadNode(const tinygltf::Node& inputNode, tinygltf::Model& gltfModel, Node* parent);
      virtual void loadMesh(Node& node, const tinygltf::Mesh& srcMesh, tinygltf::Model& gltfModel);
   protected:
      Device* _device;

      std::vector<Image*> _images;

      std::vector<Texture*> _textures;
      
      std::vector<Material> _materials;

      std::vector<Node> _nodes;

      std::string _basePath;

      std::vector<uint32_t> _indexBuffer;
      std::vector<Vertex> _vertexBuffer;

      Buffer* _vertexBufferGpu;
      Buffer* _indexBufferGpu;

   };
}