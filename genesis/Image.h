/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
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

      //! access the device
      virtual const Device* device(void) const;

      //! query parameters
      virtual int numMipMapLevels(void) const;
      virtual int width(void) const;
      virtual int height(void) const;
      virtual bool isCubeMap(void) const;

      //! get Vulkan internal
      virtual VkFormat vulkanFormat(void) const;
      virtual VkImage vulkanImage(void) const;
      virtual VkDeviceMemory vulkanDeviceMemory(void) const;
      virtual VkDeviceSize allocationSize(void) const;
   public:
      //! convert an integer sample count to the flag bits that is recognized by Vulkan
      static VkSampleCountFlagBits toSampleCountFlagBits(int sampleCount);
   protected:
      //! internal
      bool copyFromFileIntoImage(const std::string& fileName, bool srgb, uint32_t numFaces);

      bool copyFromFileIntoImageKtx(const std::string& fileName, bool srgb, uint32_t numFaces);

      bool copyFromFileIntoImageViaFreeImage(const std::string& fileName, bool srgb, uint32_t numFaces);

      //! internal
      virtual bool copyFromRawDataIntoImage(void* buffer, VkDeviceSize bufferSize, const std::vector<int>& mipMapDataOffsets, uint32_t numFaces);

      //! internal
      virtual void allocateImageAndMemory(VkImageUsageFlags usageFlags
         , VkMemoryPropertyFlags memoryPropertyFlags
         , VkImageTiling imageTiling
         , int arrayLayers, int sampleCount, bool exportMemory);

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

      VkDeviceSize _allocationSize = 0;

      static bool s_FreeImageInitialized;
   };
}