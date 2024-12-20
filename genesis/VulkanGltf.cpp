#include "VulkanGltf.h"

#include "Image.h"
#include "Texture.h"
#include "GenAssert.h"
#include "GenMath.h"
#include "Buffer.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"
#include "VkExtensions.h"
#include "VulkanInitializers.h"
#include "AccelerationStructure.h"

#include <iostream>
#include <deque>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

namespace genesis
{
   VulkanGltfModel::VulkanGltfModel(Device* device, bool rayTracing)
      : _device(device)
      , _rayTracing(rayTracing)
      , _lightInstancesGpu(nullptr)
   {
      // nothing else to do
   }

   VulkanGltfModel::~VulkanGltfModel()
   {
      for (Texture* texture : _textures)
      {
         delete texture;
      }

      for (Image* image : _images)
      {
         delete image;
      }

      for (Light* light : _lights)
      {
         delete light;
      }
      delete _lightInstancesGpu;

      delete _vertexBufferGpu;
      delete _indexBufferGpu;
   }

   bool VulkanGltfModel::isSrgb(uint32_t index) const
   {
      const auto it = _imageIndexToWhetherSrgb.find(index);
      if (it != _imageIndexToWhetherSrgb.end())
      {
         return it->second;
      }
      return false;
   }

   void VulkanGltfModel::loadImages(tinygltf::Model& glTfModel, bool srgbProcessing)
   {
      int index = -1;
      for (auto& glTFImage : glTfModel.images)
      {
         ++index;

         bool isKtx = false;
         // Image points to an external ktx file
         if (glTFImage.uri.find_last_of(".") != std::string::npos) {
            if (glTFImage.uri.substr(glTFImage.uri.find_last_of(".") + 1) == "ktx") {
               isKtx = true;
            }
         }

         const bool srgb = (srgbProcessing && isSrgb(index));
         const VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

         if (isKtx == false)
         {
            // Get the image data from the glTF loader
            unsigned char* buffer = nullptr;
            VkDeviceSize bufferSize = 0;
            bool deleteBuffer = false;
            // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
            if (glTFImage.component == 3) {
               bufferSize = glTFImage.width * glTFImage.height * 4;
               buffer = new unsigned char[bufferSize];
               unsigned char* rgba = buffer;
               const unsigned char* rgb = &glTFImage.image[0];
               for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
                  memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                  rgba += 4;
                  rgb += 3;
               }
               deleteBuffer = true;
            }
            else {
               buffer = &glTFImage.image[0];
               bufferSize = glTFImage.image.size();
               if (buffer == nullptr)
               {
                  std::cout << "Warning: " << __FUNCTION__ << ": " << "buffer==nullptr" << std::endl;
                  continue;
               }
            }

            Image* image = new Image(_device);
            std::vector<int> dataOffets = { 0 };

            image->loadFromBuffer(buffer, bufferSize, format, glTFImage.width, glTFImage.height, dataOffets);
            _images.push_back(image);

            if (deleteBuffer) {
               delete[] buffer;
            }
         }
         else
         {
            Image* image = new Image(_device);
            image->loadFromFile(_basePath + "/" + glTFImage.uri, srgb);
            _images.push_back(image);
         }
      }

      // default white image
      Image* image = new Image(_device);
      std::array<std::uint8_t, 4> buffer = { 255,255,255,255 };
      image->loadFromBuffer(buffer.data(), sizeof(std::uint8_t) * 4, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, { 0 });
      _images.push_back(image);
   }

   void VulkanGltfModel::loadTextures(tinygltf::Model& gltfModel)
   {
      int imageIndex = 0;
      for (const auto& image : _images)
      {
         _textures.push_back(new Texture(_images[imageIndex]));
         ++imageIndex;
      }
   }

   void VulkanGltfModel::addSrgbIndexIfNecessary(bool srgbProcessing, uint32_t index, bool isSrgb)
   {
      if (srgbProcessing == false || index == -1 || _imageIndexToWhetherSrgb.find(index)!=_imageIndexToWhetherSrgb.end())
      {
         return;
      }
      _imageIndexToWhetherSrgb.insert({ index, isSrgb });
   }

   void VulkanGltfModel::loadMaterials(tinygltf::Model& glTfModel, bool srgbProcessing)
   {
      const size_t totalImagesInModel = glTfModel.images.size() + 1; // +1 for default
      _materials.reserve(glTfModel.materials.size() + 1); // 1 for the default
      for (const auto& glTfMaterial : glTfModel.materials)
      {
         Material currentMaterial;

         // We only read the most basic properties required for our sample
         // Get the base color factor
         auto baseColorFactorIter = glTfMaterial.values.find("baseColorFactor");
         if (baseColorFactorIter != glTfMaterial.values.end())
         {
            currentMaterial.baseColorFactor = glm::make_vec4(baseColorFactorIter->second.ColorFactor().data());
         }

         // Get base color texture index
         auto baseColorTextureIter = glTfMaterial.values.find("baseColorTexture");

         if (baseColorTextureIter != glTfMaterial.values.end())
         {
            currentMaterial.baseColorTextureIndex = glTfModel.textures[baseColorTextureIter->second.TextureIndex()].source;
            if (currentMaterial.baseColorTextureIndex >= totalImagesInModel)
            {
               std::cout << "Warning: " << __FUNCTION__ << ": " << "currentMaterial.baseColorTextureIndex >= _textures.size()" << std::endl;
               std::cout << "\t Material: " << std::string(glTfMaterial.name) << std::endl;
               std::cout << "\t setting base color texture index to -1" << std::endl;
               currentMaterial.baseColorTextureIndex = -1;
            }
         }
         else
         {
            // assign the last (white) texture
            currentMaterial.baseColorTextureIndex = -1;
         }
         // is srgb if srgb processing is true
         addSrgbIndexIfNecessary(srgbProcessing, currentMaterial.baseColorTextureIndex, srgbProcessing);

         // emmissive
         currentMaterial.emissiveFactor = glm::vec3(glTfMaterial.emissiveFactor[0], glTfMaterial.emissiveFactor[1], glTfMaterial.emissiveFactor[2]);
         currentMaterial.emissiveTextureIndex = glTfMaterial.emissiveTexture.index;
         if (currentMaterial.emissiveTextureIndex >= (int)totalImagesInModel)
         {
            std::cout << "Warning: " << __FUNCTION__ << ": " << "currentMaterial.emissiveTextureIndex >= _textures.size()" << std::endl;
            std::cout << "\t Material: " << std::string(glTfMaterial.name) << std::endl;
            std::cout << "\t setting emmissive color texture index to -1" << std::endl;
            currentMaterial.emissiveTextureIndex = -1;
         }
         // is srgb if srgb processing is true
         addSrgbIndexIfNecessary(srgbProcessing, currentMaterial.emissiveTextureIndex, srgbProcessing);

         // Roughness and Metallic
         currentMaterial.roughness = (float)glTfMaterial.pbrMetallicRoughness.roughnessFactor;
         currentMaterial.metalness = (float)glTfMaterial.pbrMetallicRoughness.metallicFactor;
         currentMaterial.occlusionRoughnessMetalnessTextureIndex = glTfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
         if (currentMaterial.occlusionRoughnessMetalnessTextureIndex >= (int)totalImagesInModel)
         {
            std::cout << "Warning: " << __FUNCTION__ << ": " << "currentMaterial.occlusionRoughnessMetalnessTextureIndex >= _textures.size()" << std::endl;
            std::cout << "\t Material: " << std::string(glTfMaterial.name) << std::endl;
            std::cout << "\t setting base color texture index to -1" << std::endl;
            currentMaterial.occlusionRoughnessMetalnessTextureIndex = -1;
         }
         // this is never an srgb texture
         addSrgbIndexIfNecessary(srgbProcessing, currentMaterial.occlusionRoughnessMetalnessTextureIndex, false);

         auto transmissionIter = glTfMaterial.extensions.find("KHR_materials_transmission");
         if (transmissionIter != glTfMaterial.extensions.end())
         {
            currentMaterial.transmissionFactor = (float)transmissionIter->second.Get("transmissionFactor").GetNumberAsDouble();
            currentMaterial.transmissionTextureIndex = (int)transmissionIter->second.Get("transmissionTexture").GetNumberAsInt();
         }

         // Normals
         currentMaterial.normalTextureIndex = glTfMaterial.normalTexture.index;

         _materials.push_back(currentMaterial);
      }

      // default material
      Material material;
      _materials.push_back(material);
   }

   void VulkanGltfModel::loadLights(tinygltf::Model& gltfModel)
   {
      for (const auto& gltfLight : gltfModel.lights)
      {
         Light* light = new Light();
         if (gltfLight.color.size() == 3)
         {
            light->_color = Vector3_32(gltfLight.color[0], gltfLight.color[1], gltfLight.color[2]);
            if (gltfLight.type == "spot")
            {
               light->_lightType = SPOT;
            }
            else if (gltfLight.type == "directional")
            {
               light->_lightType = DIRECTIONAL;
            }
            else
            {
               light->_lightType = POINT;
            }
            light->_intensity = (float)gltfLight.intensity;
            _lights.push_back(light);
         }
      }
   }

   void VulkanGltfModel::buildLightInstancesBuffer(void)
   {
      if (_lightInstances.size() == 0)
      {
         return;
      }
      const int sizeInBytesLightInstances = (int)(_materials.size() * sizeof(LightInstance));
      _lightInstancesGpu = new Buffer(_device, BT_SBO, sizeInBytesLightInstances, true, 0, "LightInstancesGpu");
      void* pDstMaterials = _lightInstancesGpu->stagingBuffer();
      memcpy(pDstMaterials, _lightInstances.data(), sizeInBytesLightInstances);
      _lightInstancesGpu->syncToGpu(true);
   }

   void VulkanGltfModel::loadMesh(Node* node, const tinygltf::Mesh& srcMesh, tinygltf::Model& gltfModel, uint32_t fileLoadingFlags)
   {
      if (srcMesh.primitives.size() > 0)
      {
         node->_mesh = new Mesh();
      }

      // Iterate through all primitives of this node's mesh
      for (size_t i = 0; i < srcMesh.primitives.size(); i++) {
         const tinygltf::Primitive& glTFPrimitive = srcMesh.primitives[i];
         const uint32_t materialIndex = (glTFPrimitive.material == -1) ? (uint32_t)_materials.size() - 1 : glTFPrimitive.material;
         uint32_t firstIndex = static_cast<uint32_t>(_indexBuffer.size());
         uint32_t vertexStart = static_cast<uint32_t>(_vertexBuffer.size());
         uint32_t indexCount = 0;
         size_t vertexCount = 0;
         // Vertices
         {
            const float* positionBuffer = nullptr;
            const float* normalsBuffer = nullptr;
            const float* texCoordsBuffer = nullptr;

            // Get buffer data for vertex normals
            if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
               const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("POSITION")->second];
               const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
               positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
               vertexCount = accessor.count;
            }
            // Get buffer data for vertex normals
            if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
               const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
               const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
               normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            // Get buffer data for vertex texture coordinates
            // glTF supports multiple sets, we only load the first one
            if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
               const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
               const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
               texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            // Append data to model's vertex buffer
            for (size_t v = 0; v < vertexCount; v++) {
               Vertex vertex{};
               vertex.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
               vertex.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
               vertex.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
               vertex.color = glm::vec4(1.0f);

               const glm::mat4x4 fullTransform = node->fullTransform();
               if (fileLoadingFlags & FileLoadingFlags::PreTransformVertices)
               {
                  vertex.position = Vector3_32(fullTransform * Vector4_32(vertex.position, 1.0f));
                  vertex.normal = glm::normalize(Matrix3_32(fullTransform) * vertex.normal);
               }
               if (fileLoadingFlags & FlipY)
               {
                  vertex.position.y *= -1.0f;
                  vertex.normal.y *= -1.0f;
               }
               if (fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors)
               {
                  vertex.color = Vector4_32(_materials[materialIndex].baseColorFactor.x * vertex.color.x
                     , _materials[materialIndex].baseColorFactor.y * vertex.color.y
                     , _materials[materialIndex].baseColorFactor.z * vertex.color.z
                     , 1.0f
                  );
               }

               _vertexBuffer.push_back(vertex);
            }
         }
         // Indices
         {
            const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.indices];
            const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

            indexCount += static_cast<uint32_t>(accessor.count);

            // glTF supports different component types of indices
            switch (accessor.componentType) {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
               uint32_t* buf = new uint32_t[accessor.count];
               memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
               for (size_t index = 0; index < accessor.count; index++) {
                  _indexBuffer.push_back(buf[index] + vertexStart);
               }
               break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
               uint16_t* buf = new uint16_t[accessor.count];
               memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
               for (size_t index = 0; index < accessor.count; index++) {
                  _indexBuffer.push_back(buf[index] + vertexStart);
               }
               break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
               uint8_t* buf = new uint8_t[accessor.count];
               memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
               for (size_t index = 0; index < accessor.count; index++) {
                  _indexBuffer.push_back(buf[index] + vertexStart);
               }
               break;
            }
            default:
               std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
               return;
            }
         }
         Primitive primitive{};
         primitive.firstIndex = firstIndex;
         primitive.indexCount = indexCount;
         primitive.firstVertex = vertexStart;
         primitive.vertexCount = (uint32_t)vertexCount;
         primitive.materialIndex = materialIndex;
         node->_mesh->primitives.push_back(primitive);
      }
   }

   void VulkanGltfModel::loadNode(const tinygltf::Node& inputNode, tinygltf::Model& gltfModel, Node* parent, uint32_t fileLoadingFlags)
   {
      Node* node = new Node{};
      node->_matrix = Matrix4_32(1.0f);
      node->_parent = parent;

      // Get the local node matrix
      // It's either made up from translation, rotation, scale or a 4x4 matrix
      if (inputNode.translation.size() == 3) {
         node->_matrix = glm::translate(node->_matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
      }
      if (inputNode.rotation.size() == 4) {
         glm::quat q = glm::make_quat(inputNode.rotation.data());
         node->_matrix *= glm::mat4(q);
      }
      if (inputNode.scale.size() == 3) {
         node->_matrix = glm::scale(node->_matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
      }
      if (inputNode.matrix.size() == 16) {
         node->_matrix = glm::make_mat4x4(inputNode.matrix.data());
      };

      // Load node's children
      if (inputNode.children.size() > 0)
      {
         for (size_t i = 0; i < inputNode.children.size(); i++)
         {
            loadNode(gltfModel.nodes[inputNode.children[i]], gltfModel, node, fileLoadingFlags);
         }
      }

      auto lightIter = inputNode.extensions.find("KHR_lights_punctual");
      if (lightIter != inputNode.extensions.end())
      {
         auto lightlightIter = lightIter->second;
         int lightIndex = (int)lightlightIter.Get("light").GetNumberAsInt();

         if (lightIndex < _lights.size())
         {
            const Light* light = _lights[lightIndex];

            LightInstance lightInstance;
            lightInstance._light = *light;

            if (inputNode.translation.size() == 3) {
               lightInstance._position = Vector3_32(inputNode.translation[0], inputNode.translation[1], inputNode.translation[2]);
            }
            _lightInstances.push_back(lightInstance);
         }
      }

      if (inputNode.mesh > -1)
      {
         const tinygltf::Mesh& mesh = gltfModel.meshes[inputNode.mesh];
         loadMesh(node, mesh, gltfModel, fileLoadingFlags);
      }

      if (parent)
      {
         parent->_children.push_back(node);
      }
      else {
         _linearNodes.push_back(node);
      }
   }

   void VulkanGltfModel::loadScenes(tinygltf::Model& gltfModel, uint32_t fileLoadingFlags)
   {
      const tinygltf::Scene& scene = gltfModel.scenes[0];
      for (int i = 0; i < scene.nodes.size(); ++i)
      {
         const tinygltf::Node& node = gltfModel.nodes[scene.nodes[i]];
         loadNode(node, gltfModel, nullptr, fileLoadingFlags);
      }
   }

   bool loadImageDataFunc(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
   {
      // KTX files will be handled by our own code
      if (image->uri.find_last_of(".") != std::string::npos) {
         if (image->uri.substr(image->uri.find_last_of(".") + 1) == "ktx") {
            return true;
         }
      }

      return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
   }

   bool loadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
   {
      // This function will be used for samples that don't require images to be loaded
      return true;
   }

   Matrix4_32 Node::fullTransform() const
   {
      glm::mat4 m = _matrix;
      Node* p = _parent;
      while (p) {
         m = p->_matrix * m;
         p = p->_parent;
      }
      return m;
   }

   void VulkanGltfModel::loadFromFile(const std::string& fileName, uint32_t fileLoadingFlags)
   {
      tinygltf::Model glTfModel;
      tinygltf::TinyGLTF gltfContext;
      std::string error, warning;

      if (fileLoadingFlags & FileLoadingFlags::DontLoadImages) {
         gltfContext.SetImageLoader(loadImageDataFuncEmpty, nullptr);
      }
      else {
         gltfContext.SetImageLoader(loadImageDataFunc, nullptr);
      }

      bool fileLoaded = false;
      if (fileName.find(".gltf") != std::string::npos)
      {
         fileLoaded = gltfContext.LoadASCIIFromFile(&glTfModel, &error, &warning, fileName);
      }
      else if (fileName.find(".glb") !=std::string::npos)
      {
         fileLoaded = gltfContext.LoadBinaryFromFile(&glTfModel, &error, &warning, fileName);
      }

      if (fileLoaded == false)
      {
         std::cout << "Warning: " << __FUNCTION__ << ": " << "could not load file: " << fileName << std::endl;
         return;
      }

      _basePath = fileName.substr(0, fileName.find_last_of("/\\"));

      const bool srgbProcessing = fileLoadingFlags & ColorTexturesAreSrgb;

      loadMaterials(glTfModel, srgbProcessing);

      if (!(fileLoadingFlags & FileLoadingFlags::DontLoadImages))
      {
         loadImages(glTfModel, srgbProcessing);
      }

      loadTextures(glTfModel);
      loadLights(glTfModel);
      loadScenes(glTfModel, fileLoadingFlags);

      buildLightInstancesBuffer();
      
      VkBufferUsageFlags additionalFlags = 0;
      //if (_rayTracing)
      {
         additionalFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR 
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      }

      {
         const int sizeOfVertexBuffer = (int)(_vertexBuffer.size() * sizeof(Vertex));

         _vertexBufferGpu = new Buffer(_device, BT_VERTEX_BUFFER, sizeOfVertexBuffer, true, additionalFlags, "VulkanGltfModel::_vertexBufferGpu");

         uint8_t* pDstData = (uint8_t*)_vertexBufferGpu->stagingBuffer();
         memcpy(pDstData, _vertexBuffer.data(), sizeOfVertexBuffer);
         _vertexBufferGpu->syncToGpu(true);
      }

      {
         const int sizeOfIndexBuffer = (int)(_indexBuffer.size() * sizeof(uint32_t));

         _indexBufferGpu = new Buffer(_device, BT_INDEX_BUFFER, sizeOfIndexBuffer, true, additionalFlags, "VulkanGltfModel::_indexBufferGpu");

         uint8_t* pDstData = (uint8_t*)_indexBufferGpu->stagingBuffer();
         memcpy(pDstData, _indexBuffer.data(), sizeOfIndexBuffer);
         _indexBufferGpu->syncToGpu(true);
      }
   }

   const std::vector<Image*>& VulkanGltfModel::images(void) const
   {
      return _images;
   }

   const Buffer* VulkanGltfModel::vertexBuffer(void) const
   {
      return _vertexBufferGpu;
   }
   const Buffer* VulkanGltfModel::indexBuffer(void) const
   {
      return _indexBufferGpu;
   }

   int VulkanGltfModel::numVertices() const
   {
      return (int)_vertexBuffer.size();
   }

   const std::vector<Node*>& VulkanGltfModel::linearNodes(void) const
   {
      return _linearNodes;
   }

   const std::vector<Texture*>& VulkanGltfModel::textures(void) const
   {
      return _textures;
   }

   const std::vector<Material>& VulkanGltfModel::materials(void) const
   {
      return _materials;
   }

   int VulkanGltfModel::numPrimitives(void) const
   {
      int count = 0;
      forEachPrimitive(
         [&](const Primitive& primitive)
         {
            ++count;
         }
      );
      return count;
   }

   void VulkanGltfModel::forEachPrimitive(const std::function<void(const Primitive&)>& func) const
   {
      std::deque<const Node*> nodesToProcess;
      for (const Node* node : linearNodes())
      {
         nodesToProcess.push_back(node);
      }

      while (!nodesToProcess.empty())
      {
         const Node* node = nodesToProcess.front(); nodesToProcess.pop_front();

         if (node->_mesh)
         {
            for (const Primitive& primitive : node->_mesh->primitives)
            {
               if (primitive.indexCount > 0)
               {
                  func(primitive);
               }
            }
         }

         for (const Node* child : node->_children)
         {
            nodesToProcess.push_back(child);
         }
      }
   }

}