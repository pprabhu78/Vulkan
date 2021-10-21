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
      virtual bool loadFromFile(const std::string& fileName);

      virtual bool loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, int width, int height, const std::vector<int>& mipMapDataOffsets);

      virtual const Device* device(void) const;

      virtual int numMipMapLevels(void) const;
      virtual int width(void) const;
      virtual int height(void) const;

      virtual VkFormat vulkanFormat(void) const;
      virtual VkImage vulkanImage(void) const;
   protected:
      virtual bool copyFromFileIntoImage(const std::string& fileName);

      virtual bool copyFromRawDataIntoImage(void* buffer, VkDeviceSize bufferSize, const std::vector<int>& mipMapDataOffsets);

   protected:
      Device* _device;

      VkDeviceMemory _deviceMemory;
      VkImage _image;

      int _width;
      int _height;
      int _numMipMapLevels;

      VkFormat _format;
   };
}