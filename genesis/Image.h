#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace genesis
{
   class Device;

   class Image
   {
   public:
      Image(Device* device);

      virtual ~Image();

   public:
      //! load from file
      virtual bool loadFromFile(const std::string& fileName, bool srgb);

      //! load from file. cube map
      virtual bool loadFromFileCubeMap(const std::string& fileName);

      //! load from buffer
      virtual bool loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, int width, int height, const std::vector<int>& mipMapDataOffsets);

      virtual const Device* device(void) const;

      //! query parameters
      virtual int numMipMapLevels(void) const;
      virtual int width(void) const;
      virtual int height(void) const;
      virtual bool isCubeMap(void) const;

      //! get vulkan internal
      virtual VkFormat vulkanFormat(void) const;
      virtual VkImage vulkanImage(void) const;
      virtual VkDeviceMemory vulkanDeviceMemory(void) const;
   protected:
      //! internal
      virtual bool copyFromFileIntoImage(const std::string& fileName, bool srgb, uint32_t numFaces);

      //! internal
      virtual bool copyFromRawDataIntoImage(void* buffer, VkDeviceSize bufferSize, const std::vector<int>& mipMapDataOffsets, uint32_t numFaces);

      //! internal
      virtual void allocateImageAndMemory(VkImageUsageFlags usageFlags
         , VkMemoryPropertyFlags memoryPropertyFlags
         , VkImageTiling imageTiling
         , int numFaces);

      //! internal
      virtual void generateMipMaps(void);

   protected:
      Device* _device;

      VkFormat _format;

      VkImage _image;
      VkDeviceMemory _deviceMemory;

      int _width;
      int _height;
      int _numMipMapLevels;
      bool _isCubeMap = false;
   };
}