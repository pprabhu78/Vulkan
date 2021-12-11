#include "VulkanGltf.h"

#include "Image.h"
#include "Texture.h"
#include "GenAssert.h"
#include "GenMath.h"
#include "Buffer.h"
#include "Device.h"
#include "VulkanInitializers.h"
#include "VulkanTools.h"
#include "VulkanFunctions.h"
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
   VulkanGltfModel::VulkanGltfModel(Device* device, bool indirect, bool rayTracing)
      : _device(device)
      , _indirectBufferGpu(nullptr)
      , _indirect(indirect)
      , _materialIndicesGpu(nullptr)
      , _indexIndicesGpu(nullptr)
      , _materialsGpu(nullptr)
      , _rayTracing(rayTracing)
      , _blas(nullptr)
      , _tlas(nullptr)
      , _lightInstancesGpu(nullptr)
   {

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

      delete _materialIndicesGpu;
      delete _materialsGpu;

      vkDestroyDescriptorPool(_device->vulkanDevice(), _descriptorPool, nullptr);
      vkDestroyDescriptorSetLayout(_device->vulkanDevice(), _descriptorSetLayout, nullptr);

      delete _indirectBufferGpu;

      delete _indexIndicesGpu;
      delete _tlas;
      delete _blas;
   }

   void VulkanGltfModel::loadImages(tinygltf::Model& glTfModel)
   {
      for (auto& glTFImage : glTfModel.images)
      {
         bool isKtx = false;
         // Image points to an external ktx file
         if (glTFImage.uri.find_last_of(".") != std::string::npos) {
            if (glTFImage.uri.substr(glTFImage.uri.find_last_of(".") + 1) == "ktx") {
               isKtx = true;
            }
         }

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
            }

            Image* image = new Image(_device);
            std::vector<int> dataOffets = { 0 };

            image->loadFromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, dataOffets);
            _images.push_back(image);

            if (deleteBuffer) {
               delete[] buffer;
            }
         }
         else
         {
            Image* image = new Image(_device);
            image->loadFromFile(_basePath + "/" + glTFImage.uri);
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
   void VulkanGltfModel::loadMaterials(tinygltf::Model& glTfModel)
   {
      _materials.resize(glTfModel.materials.size());

      int index = 0;
      for (const auto& glTfMaterial : glTfModel.materials)
      {
         // We only read the most basic properties required for our sample
         // Get the base color factor
         auto baseColorFactorIter = glTfMaterial.values.find("baseColorFactor");
         if (baseColorFactorIter != glTfMaterial.values.end())
         {
            _materials[index].baseColorFactor = glm::make_vec4(baseColorFactorIter->second.ColorFactor().data());
         }

         _materials[index].emissiveFactor = Vector4_32(glTfMaterial.emissiveFactor[0], glTfMaterial.emissiveFactor[1], glTfMaterial.emissiveFactor[2], 1);

         // Get base color texture index
         auto baseColorTextureIter = glTfMaterial.values.find("baseColorTexture");
         if (baseColorTextureIter != glTfMaterial.values.end())
         {
            _materials[index].baseColorTextureIndex = glTfModel.textures[baseColorTextureIter->second.TextureIndex()].source;
         }
         else
         {
            // assign the last (white) texture
            _materials[index].baseColorTextureIndex = (int)(_textures.size() - 1);
         }
         ++index;
      }
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
      _lightInstancesGpu = new Buffer(_device, BT_SBO, sizeInBytesLightInstances, true, BufferProperties(), "LightInstancesGpu");
      void* pDstMaterials = _lightInstancesGpu->stagingBuffer();
      memcpy(pDstMaterials, _lightInstances.data(), sizeInBytesLightInstances);
      _lightInstancesGpu->syncToGpu(true);
   }

   void VulkanGltfModel::loadMesh(Node* node, const tinygltf::Mesh& srcMesh, tinygltf::Model& gltfModel)
   {
      if (srcMesh.primitives.size() > 0)
      {
         node->_mesh = new Mesh();
      }

      // Iterate through all primitives of this node's mesh
      for (size_t i = 0; i < srcMesh.primitives.size(); i++) {
         const tinygltf::Primitive& glTFPrimitive = srcMesh.primitives[i];
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
               Vertex vert{};
               vert.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
               vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
               vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
               vert.color = glm::vec4(1.0f);
               _vertexBuffer.push_back(vert);
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
         primitive.materialIndex = glTFPrimitive.material;
         node->_mesh->primitives.push_back(primitive);
      }
   }

   void VulkanGltfModel::loadNode(const tinygltf::Node& inputNode, tinygltf::Model& gltfModel, Node* parent)
   {
      Node* node = new Node{};
      node->_matrix = Matrix4_32(1.0f);

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
            loadNode(gltfModel.nodes[inputNode.children[i]], gltfModel, node);
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
         loadMesh(node, mesh, gltfModel);
      }

      if (parent)
      {
         parent->_children.push_back(node);
      }
      else {
         _linearNodes.push_back(node);
      }
   }

   void VulkanGltfModel::loadScenes(tinygltf::Model& gltfModel)
   {
      const tinygltf::Scene& scene = gltfModel.scenes[0];
      for (int i = 0; i < scene.nodes.size(); ++i)
      {
         const tinygltf::Node& node = gltfModel.nodes[scene.nodes[i]];
         loadNode(node, gltfModel, nullptr);
      }
   }

   void VulkanGltfModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node* node) const
   {
      if (node->_mesh == nullptr)
      {
         return;
      }

      for (const Primitive& primitive : node->_mesh->primitives)
      {
         if (primitive.indexCount > 0)
         {
            const Material& material = _materials[primitive.materialIndex];
            const int textureIndex = material.baseColorTextureIndex;

            // Bind descriptor sets describing shader binding points
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &_vecDescriptorSets[textureIndex], 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
         }
      }

      // draw the children
      for (const auto& child : node->_children) {
         draw(commandBuffer, pipelineLayout, child);
      }
   }

   void VulkanGltfModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
   {
      // Bind triangle vertex buffer (contains position and colors)
      VkDeviceSize offsets[1] = { 0 };
      VkBuffer buffer = vertexBuffer()->vulkanBuffer();
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, offsets);

      // Bind triangle index buffer
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer()->vulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

      if (_indirect)
      {
         std::uint32_t firstSet = 1;
         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, std::uint32_t(_vecDescriptorSets.size()), _vecDescriptorSets.data(), 0, nullptr);
         vkCmdDrawIndexedIndirect(commandBuffer, _indirectBufferGpu->vulkanBuffer(), 0, (uint32_t)_indirectCommands.size(), sizeof(VkDrawIndexedIndirectCommand));
      }
      else
      {
         for (const Node* node : _linearNodes)
         {
            draw(commandBuffer, pipelineLayout, node);
         }
      }
   }

   const std::vector<VkDescriptorSet>& VulkanGltfModel::descriptorSets(void) const
   {
      return _vecDescriptorSets;
   }

   void VulkanGltfModel::buildIndirectBuffer(void)
   {
      std::deque<const Node*> nodesToProcess;
      for (const Node* node : _linearNodes)
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
                  VkDrawIndexedIndirectCommand command;
                  command.indexCount = primitive.indexCount;
                  command.instanceCount = 1;
                  command.firstIndex = primitive.firstIndex;
                  command.vertexOffset = 0;
                  command.firstInstance = 0;
                  _indirectCommands.push_back(command);
                  _materialIndices.push_back(primitive.materialIndex);
                  _indexIndices.push_back(primitive.firstIndex);
               }
            }
         }

         for (const auto& child : node->_children)
         {
            nodesToProcess.push_back(node);
         }
      }

      const int sizeInBytes = (int)(_indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
      _indirectBufferGpu = new Buffer(_device, BT_INDIRECT_BUFFER, sizeInBytes, true);
      void* pDst = _indirectBufferGpu->stagingBuffer();
      memcpy(pDst, _indirectCommands.data(), sizeInBytes);
      _indirectBufferGpu->syncToGpu(true);

      const int sizeInBytesMaterialIndices = (int)(_materialIndices.size() * sizeof(uint32_t));
      _materialIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterialIndices, true);
      void* pDstMaterialIndices = _materialIndicesGpu->stagingBuffer();
      memcpy(pDstMaterialIndices, _materialIndices.data(), sizeInBytesMaterialIndices);
      _materialIndicesGpu->syncToGpu(true);

      const int sizeInBytesIndexIndices = (int)(_indexIndices.size() * sizeof(uint32_t));
      _indexIndicesGpu = new Buffer(_device, BT_SBO, sizeInBytesIndexIndices, true);
      void* pDstIndexIndices = _indexIndicesGpu->stagingBuffer();
      memcpy(pDstIndexIndices, _indexIndices.data(), sizeInBytesIndexIndices);
      _indexIndicesGpu->syncToGpu(true);

      const int sizeInBytesMaterials = (int)(_materials.size() * sizeof(Material));
      _materialsGpu = new Buffer(_device, BT_SBO, sizeInBytesMaterials, true, BufferProperties(), "MaterialsGpu");
      void* pDstMaterials = _materialsGpu->stagingBuffer();
      memcpy(pDstMaterials, _materials.data(), sizeInBytesMaterials);
      _materialsGpu->syncToGpu(true);

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

   void VulkanGltfModel::bakeAttributes(tinygltf::Model& gltfModel, uint32_t fileLoadingFlags)
   {
      const bool preTransformVertices = fileLoadingFlags & FileLoadingFlags::PreTransformVertices;
      const bool flipY = fileLoadingFlags & FlipY;
      const bool premultiplyColors = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
      if (!(flipY || premultiplyColors || preTransformVertices))
      {
         return;
      }

      for (const Node* node : _linearNodes)
      {
         if (node->_mesh == nullptr)
         {
            continue;
         }

         const Matrix4_32 fullTransform = node->fullTransform();
         for (const Primitive& primitive : node->_mesh->primitives)
         {
            for (uint32_t i = 0; i < primitive.vertexCount; ++i)
            {
               Vertex& vertex = _vertexBuffer[primitive.firstVertex + i];

               if (preTransformVertices)
               {
                  vertex.position = Vector3_32(fullTransform * Vector4_32(vertex.position, 1.0f));
                  vertex.normal = glm::normalize(Matrix3_32(fullTransform) * vertex.normal);
               }
               if (flipY)
               {
                  vertex.position.y *= -1.0f;
                  vertex.normal.y *= -1.0f;
               }
               if (premultiplyColors)
               {
                  vertex.color = Vector4_32(_materials[primitive.materialIndex].baseColorFactor.x * vertex.color.x
                     , _materials[primitive.materialIndex].baseColorFactor.y * vertex.color.y
                     , _materials[primitive.materialIndex].baseColorFactor.z * vertex.color.z
                     , 1.0f
                  );
               }
            }
         }
      }
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

      bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTfModel, &error, &warning, fileName);
      if (fileLoaded == false)
      {
         return;
      }

      _basePath = fileName.substr(0, fileName.find_last_of('/'));

      if (!(fileLoadingFlags & FileLoadingFlags::DontLoadImages))
      {
         loadImages(glTfModel);
      }

      loadTextures(glTfModel);
      loadMaterials(glTfModel);
      loadLights(glTfModel);
      loadScenes(glTfModel);

      buildLightInstancesBuffer();
      
      bakeAttributes(glTfModel, fileLoadingFlags);

      BufferProperties props;
      if (_rayTracing)
      {
         props._deviceAddressing = true;
         props._inputToAccelerationStructure = true;
         props._vertexOrIndexBoundAsSsbo = true;
      }

      {
         const int sizeOfVertexBuffer = (int)(_vertexBuffer.size() * sizeof(Vertex));

         _vertexBufferGpu = new Buffer(_device, BT_VERTEX_BUFFER, sizeOfVertexBuffer, true, props, "VulkanGltfModel::_vertexBufferGpu");

         uint8_t* pDstData = (uint8_t*)_vertexBufferGpu->stagingBuffer();
         memcpy(pDstData, _vertexBuffer.data(), sizeOfVertexBuffer);
         _vertexBufferGpu->syncToGpu(true);
      }

      {
         const int sizeOfIndexBuffer = (int)(_indexBuffer.size() * sizeof(uint32_t));

         _indexBufferGpu = new Buffer(_device, BT_INDEX_BUFFER, sizeOfIndexBuffer, true, props, "VulkanGltfModel::_indexBufferGpu");

         uint8_t* pDstData = (uint8_t*)_indexBufferGpu->stagingBuffer();
         memcpy(pDstData, _indexBuffer.data(), sizeOfIndexBuffer);
         _indexBufferGpu->syncToGpu(true);
      }

      if (_indirect)
      {
         buildIndirectBuffer();
      }

      setupDescriptorPool();
      setupDescriptorSetLayout();
      updateDescriptorSets();
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

   void VulkanGltfModel::setupDescriptorPool()
   {
      std::vector<VkDescriptorPoolSize> poolSizes = {};

      uint32_t maxSets = 0;
      if (_indirect)
      {
         poolSizes.push_back(genesis::vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3));
         poolSizes.push_back(genesis::vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)_textures.size()));

         maxSets = 1;
      }
      else
      {
         poolSizes.push_back(genesis::vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)_textures.size()));

         maxSets = (uint32_t)_textures.size();
      }

      VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = genesis::vulkanInitializers::descriptorPoolCreateInfo(poolSizes, maxSets);
      VK_CHECK_RESULT(vkCreateDescriptorPool(_device->vulkanDevice(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool));
   }

   void VulkanGltfModel::setupDescriptorSetLayout(void)
   {
      std::vector<VkDescriptorSetLayoutBinding> setBindings = {};
      uint32_t bindingIndex = 0;

      VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;

      if (_indirect)
      {
         setBindings.push_back(genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         setBindings.push_back(genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         setBindings.push_back(genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, 1));
         setBindings.push_back(genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++, static_cast<uint32_t>(_textures.size())));

         descriptorSetLayoutCreateInfo = genesis::vulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

         // additional flags to specify the last variable binding point
         VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
         setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
         setLayoutBindingFlags.bindingCount = (uint32_t)setBindings.size();
         std::vector<VkDescriptorBindingFlags> descriptorBindingFlags = { 0, 0, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
         setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

         descriptorSetLayoutCreateInfo.pNext = &setLayoutBindingFlags;

         descriptorSetLayoutCreateInfo = genesis::vulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

      }
      else
      {
         setBindings.push_back(genesis::vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, bindingIndex++));
         descriptorSetLayoutCreateInfo = genesis::vulkanInitializers::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));
      }

      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_device->vulkanDevice(), &descriptorSetLayoutCreateInfo, nullptr, &_descriptorSetLayout));
   }

   void VulkanGltfModel::updateDescriptorSets()
   {
      if (_indirect)
      {
         uint32_t variableDescCounts[] = { static_cast<uint32_t>(_textures.size()) };
         VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
         variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
         variableDescriptorCountAllocInfo.descriptorSetCount = 1;
         variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

         VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::vulkanInitializers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);
         descriptorSetAllocateInfo.pNext = &variableDescriptorCountAllocInfo;

         VkDescriptorSet descriptorSet;
         vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

         std::vector<VkDescriptorImageInfo> textureDescriptors;
         textureDescriptors.reserve(_textures.size());
         for (const Texture* texture : _textures)
         {
            textureDescriptors.push_back(texture->descriptor());
         }


         const int numBindings = 4;
         std::array<VkWriteDescriptorSet, numBindings> writeDescriptorSet = {};

         int bindingIndex = -1;
         writeDescriptorSet[0] = vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ++bindingIndex, &_materialsGpu->descriptor());
         writeDescriptorSet[1] = vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ++bindingIndex, &_materialIndicesGpu->descriptor());
         writeDescriptorSet[2] = vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ++bindingIndex, &_indexIndicesGpu->descriptor());

         writeDescriptorSet[3] = {};
         writeDescriptorSet[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         writeDescriptorSet[3].dstBinding = ++bindingIndex;
         writeDescriptorSet[3].dstArrayElement = 0;
         writeDescriptorSet[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         writeDescriptorSet[3].descriptorCount = static_cast<uint32_t>(_textures.size());
         writeDescriptorSet[3].pBufferInfo = 0;
         writeDescriptorSet[3].dstSet = descriptorSet;
         writeDescriptorSet[3].pImageInfo = textureDescriptors.data();

         vkUpdateDescriptorSets(_device->vulkanDevice(), (int)writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);

         _vecDescriptorSets.push_back(descriptorSet);
      }
      else
      {
         for (int i = 0; i < _textures.size(); ++i)
         {
            const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = genesis::vulkanInitializers::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);

            // allocate a set
            VkDescriptorSet descriptorSet;
            vkAllocateDescriptorSets(_device->vulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);

            std::vector<VkWriteDescriptorSet> writeDescriptorSets =
            {
            genesis::vulkanInitializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &_textures[i]->descriptor())
            };

            vkUpdateDescriptorSets(_device->vulkanDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

            _vecDescriptorSets.push_back(descriptorSet);
         }
      }
   }

   VkDescriptorSetLayout VulkanGltfModel::vulkanDescriptorSetLayout(void) const
   {
      return _descriptorSetLayout;
   }

   void VulkanGltfModel::buildBlas(void)
   {
      VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
      VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

      vertexBufferDeviceAddress.deviceAddress = _vertexBufferGpu->bufferAddress();
      indexBufferDeviceAddress.deviceAddress = _indexBufferGpu->bufferAddress();

      VkAccelerationStructureGeometryTrianglesDataKHR    triangles{};
      triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
      triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
      triangles.vertexData = vertexBufferDeviceAddress;
      triangles.vertexStride = (uint32_t)sizeof(Vertex);
      triangles.maxVertex = (uint32_t)_vertexBuffer.size();
      triangles.indexType = VK_INDEX_TYPE_UINT32;
      triangles.indexData = indexBufferDeviceAddress;

      VkAccelerationStructureGeometryKHR accelerationStructureGeometry = vks::initializers::accelerationStructureGeometryKHR();
      accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
      accelerationStructureGeometry.geometry.triangles = triangles;

      VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
      
      std::vector<VkAccelerationStructureGeometryKHR> vecAccelerationStructureGeometries;
      std::vector<VkAccelerationStructureBuildRangeInfoKHR> vecAccelerationStructureBuildRangeInfos;
      std::vector<uint32_t> primitiveCounts;

      std::deque<const Node*> nodesToProcess;
      for (const Node* node : _linearNodes)
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
                  accelerationStructureBuildRangeInfo.primitiveCount = primitive.indexCount/3;
                  accelerationStructureBuildRangeInfo.primitiveOffset = primitive.firstIndex*sizeof(uint32_t);
                  accelerationStructureBuildRangeInfo.firstVertex = 0;
                  accelerationStructureBuildRangeInfo.transformOffset = 0;

                  vecAccelerationStructureGeometries.push_back(accelerationStructureGeometry);
                  vecAccelerationStructureBuildRangeInfos.push_back(accelerationStructureBuildRangeInfo);
                  primitiveCounts.push_back(accelerationStructureBuildRangeInfo.primitiveCount);
               }
            }
         }

         for (const auto& child : node->_children)
         {
            nodesToProcess.push_back(node);
         }
      }

      // Get size info
      VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
      accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationStructureBuildGeometryInfo.geometryCount = (uint32_t)vecAccelerationStructureGeometries.size();
      accelerationStructureBuildGeometryInfo.pGeometries = vecAccelerationStructureGeometries.data();

      VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = vks::initializers::accelerationStructureBuildSizesInfoKHR();
      vkGetAccelerationStructureBuildSizesKHR(
         _device->vulkanDevice(),
         VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
         &accelerationStructureBuildGeometryInfo,
         primitiveCounts.data(),
         &accelerationStructureBuildSizesInfo);

      _blas = new AccelerationStructure(_device, BLAS, accelerationStructureBuildSizesInfo.accelerationStructureSize);

      BufferProperties props;
      props._deviceAddressing = true;

      Buffer* scratchBuffer = new Buffer(_device, BT_SBO, (int)accelerationStructureBuildSizesInfo.buildScratchSize, false, props);

      VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
      accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      accelerationBuildGeometryInfo.dstAccelerationStructure = _blas->handle();
      accelerationBuildGeometryInfo.geometryCount = (uint32_t)vecAccelerationStructureGeometries.size();
      accelerationBuildGeometryInfo.pGeometries = vecAccelerationStructureGeometries.data();
      accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer->bufferAddress();

      std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { vecAccelerationStructureBuildRangeInfos.data() };

      // Build the acceleration structure on the device via a one-time command buffer submission
      // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
      VkCommandBuffer commandBuffer = _device->getCommandBuffer(true);
      vkCmdBuildAccelerationStructuresKHR(
         commandBuffer,
         1,
         &accelerationBuildGeometryInfo
         , accelerationBuildStructureRangeInfos.data());
      _device->flushCommandBuffer(commandBuffer);

      delete scratchBuffer;
   }

   const AccelerationStructure* VulkanGltfModel::blas(void) const
   {
      return _blas;
   }

   void VulkanGltfModel::buildTlas(void)
   {

   }
   const AccelerationStructure* VulkanGltfModel::tlas(void) const
   {
      return _tlas;
   }
}