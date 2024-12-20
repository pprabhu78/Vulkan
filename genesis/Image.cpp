/*
* Copyright (C) 2021-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Image.h"
#include "Device.h"
#include "PhysicalDevice.h"
#include "Buffer.h"
#include "VulkanInitializers.h"
#include "VulkanDebug.h"
#include "ImageTransitions.h"

#include "GenAssert.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include <FreeImage.h>

#include "tiffio.h"
#include "tiffiop.h"

#include <algorithm>
#include <fstream>

namespace genesis
{
	VkSampleCountFlagBits Image::toSampleCountFlagBits(int sampleCount)
	{
		if (sampleCount == 1) return VK_SAMPLE_COUNT_1_BIT;
		if (sampleCount == 2) return VK_SAMPLE_COUNT_2_BIT;
		if (sampleCount == 4) return VK_SAMPLE_COUNT_4_BIT;
		if (sampleCount == 8) return VK_SAMPLE_COUNT_8_BIT;
		if (sampleCount == 16) return VK_SAMPLE_COUNT_16_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}

	Image::Image(Device* _device)
		: _device(_device)
	{

	}

	Image::~Image()
	{
		VkDevice vulkanDevice = _device->vulkanDevice();

		vkDestroyImage(vulkanDevice, _image, nullptr);
		vkFreeMemory(vulkanDevice, _deviceMemory, nullptr);
	}

	void Image::allocateImageAndMemory(VkImageUsageFlags usageFlags
		, VkMemoryPropertyFlags memoryPropertyFlags
		, VkImageTiling imageTiling
		, int arrayLayers, int sampleCount, bool exportMemory)
	{
		VkImageCreateInfo imageCreateInfo = vkInitializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = _format;
		imageCreateInfo.extent = { static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), 1 };
		imageCreateInfo.mipLevels = _numMipMapLevels;
		imageCreateInfo.arrayLayers = arrayLayers;
		imageCreateInfo.samples = toSampleCountFlagBits(sampleCount);
		imageCreateInfo.tiling = imageTiling;
		imageCreateInfo.usage = usageFlags;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (arrayLayers == 6)
		{
			imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
		if (exportMemory)
		{
			externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
			imageCreateInfo.pNext = &externalMemoryImageCreateInfo;
		}

		VK_CHECK_RESULT(vkCreateImage(_device->vulkanDevice(), &imageCreateInfo, nullptr, &_image));

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(_device->vulkanDevice(), _image, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo = genesis::vkInitializers::memoryAllocateInfo();
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = _device->physicalDevice()->getMemoryTypeIndex(memoryRequirements.memoryTypeBits
			, memoryPropertyFlags);

		VkExportMemoryAllocateInfo exportMemoryAllocateInfo{};
		exportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
		exportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
		if (exportMemory)
		{
			memoryAllocateInfo.pNext = &exportMemoryAllocateInfo;
		}

		VK_CHECK_RESULT(vkAllocateMemory(_device->vulkanDevice(), &memoryAllocateInfo, nullptr, &_deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(_device->vulkanDevice(), _image, _deviceMemory, 0));

		// save out the allocation size
		_allocationSize = memoryRequirements.size;
	}

	bool Image::copyFromRawDataIntoImage(void* pSrcData, VkDeviceSize pSrcDataSize, const std::vector<int>& mipMapDataOffsetsAllFaces, uint32_t numFaces)
	{
		VulkanBuffer* stagingBuffer = new VulkanBuffer(_device
			, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			, (int)pSrcDataSize);
		uint8_t* pDstData = 0;
		VK_CHECK_RESULT(vkMapMemory(_device->vulkanDevice(), stagingBuffer->_deviceMemory, 0, pSrcDataSize, 0, (void**)&pDstData));
		memcpy(pDstData, pSrcData, pSrcDataSize);
		vkUnmapMemory(_device->vulkanDevice(), stagingBuffer->_deviceMemory);

		const bool generatingMipMaps = (mipMapDataOffsetsAllFaces.size() / numFaces != _numMipMapLevels);

		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (generatingMipMaps)
		{
			imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		allocateImageAndMemory(imageUsageFlags
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT // on the gpu
			, VK_IMAGE_TILING_OPTIMAL, numFaces, 1, false);

		std::vector<VkBufferImageCopy> bufferCopyRegions;

		const uint32_t numMipMaps = (uint32_t)mipMapDataOffsetsAllFaces.size() / numFaces;

		for (uint32_t face = 0; face < numFaces; ++face)
		{
			for (uint32_t mipLevel = 0; mipLevel < numMipMaps; ++mipLevel)
			{
				VkBufferImageCopy bufferImageCopy = {};

				const uint32_t offsetIntoMipData = (numMipMaps * face) + mipLevel;
				bufferImageCopy.bufferOffset = mipMapDataOffsetsAllFaces[offsetIntoMipData];
				bufferImageCopy.bufferRowLength = 0;
				bufferImageCopy.bufferImageHeight = 0;

				bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferImageCopy.imageSubresource.mipLevel = mipLevel;
				bufferImageCopy.imageSubresource.baseArrayLayer = face;
				bufferImageCopy.imageSubresource.layerCount = 1;

				bufferImageCopy.imageOffset = { 0,0,0 };
				bufferImageCopy.imageExtent.width = _width >> mipLevel;
				bufferImageCopy.imageExtent.height = _height >> mipLevel;
				bufferImageCopy.imageExtent.depth = 1;

				bufferCopyRegions.push_back(bufferImageCopy);
			};
		}

		VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT , 0, numMipMaps, 0, numFaces };

		transitions::setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

		vkCmdCopyBufferToImage(commandBuffer
			, stagingBuffer->vulkanBuffer()
			, _image
			, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			, (uint32_t)bufferCopyRegions.size()
			, bufferCopyRegions.data()
		);

		VkImageLayout newImageLayout = (generatingMipMaps) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		transitions::setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, newImageLayout, subresourceRange);

		_device->flushCommandBuffer(commandBuffer);

		delete stagingBuffer;

		_isCubeMap = (numFaces == 6);

		return true;
	}

	VkFormat toVulkanFormat(uint32_t glInternalFormat, bool srgb)
	{
		// This is GL_RGBA8
		if (glInternalFormat == 32856)
		{
			if (srgb)
			{
				return VK_FORMAT_R8G8B8A8_SRGB;
			}
			else
			{
				return VK_FORMAT_R8G8B8A8_UNORM;
			}
		}

		// This is GL_RGBA16F_ARB
		if (glInternalFormat == 34842)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}

		std::cout << __FUNCTION__ << ": unknown format!" << std::endl;
		return VK_FORMAT_UNDEFINED;
	}

	bool Image::s_FreeImageInitialized = false;

	void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char* message) {
		std::cout << "error: " << message << std::endl;
	}

	bool Image::copyFromFileIntoImageViaLibTiff(const std::string& fileName, bool srgb, uint32_t numFaces)
	{
		TIFF* tif = TIFFOpen(fileName.c_str(), "r");
		if (tif == nullptr)
		{
			std::cout << "could not load: " << fileName << std::endl;
			return false;
		}

		GEN_ASSERT(isTiled(tif) == false);

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &_width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &_height);

		int tileWidth = 0;
		int tileHeight = 0;
		int compressionType = 0;
		int planarConfig = 0;
		int samplesPerPixel = 0;
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileHeight);
		TIFFGetField(tif, TIFFTAG_COMPRESSION, &compressionType);
		TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

		int bps = 0;
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bps);

		if (bps == 8)
		{
			_format = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			std::cout << "could not load: " << fileName << std::endl;
			return false;
		}

		_numMipMapLevels = 1;

		std::vector<std::uint8_t> pixelData;
		pixelData.resize(_width * _height * 4);

		std::uint8_t* dstPointer = pixelData.data();

		tmsize_t stripSize = TIFFStripSize(tif);
		tdata_t srcBuffer = _TIFFmalloc(stripSize);
		uint32_t numStrips = TIFFNumberOfStrips(tif);
		int rowsPerStrip = (int)stripSize / (_width * 4);
		for (tstrip_t currentStrip = 0; currentStrip < numStrips; currentStrip++)
		{
			TIFFReadEncodedStrip(tif, currentStrip, srcBuffer, (tsize_t)-1);

			if (currentStrip == numStrips - 1)
			{
				int rowsRemaining = _height - rowsPerStrip * currentStrip;
				memcpy(dstPointer, srcBuffer, rowsRemaining*_width*4);
			}
			else
			{
				memcpy(dstPointer, srcBuffer, stripSize);
			}
			
			dstPointer += stripSize;
		}
		_TIFFfree(srcBuffer);
		TIFFClose(tif);

		std::vector<std::uint8_t> flippedPixelData;
		flippedPixelData.resize(pixelData.size());
		for (size_t r = 0; r < _height; r++) {
			auto src = &pixelData[r * 4 * _width];
			auto dst = &flippedPixelData[(_height - r - 1) * 4 * _width];
			std::copy(src, src + 4 * _width, dst);
		}

		pixelData.swap(flippedPixelData);

		std::vector<int> dataOffsets = { 0 };
		copyFromRawDataIntoImage(pixelData.data(), pixelData.size(), dataOffsets, 1);

		return true;
	}


	bool Image::copyFromFileIntoImageViaFreeImage(const std::string& fileName, bool srgb, uint32_t numFaces)
	{
		if (!s_FreeImageInitialized)
		{
			FreeImage_Initialise();
			FreeImage_SetOutputMessage(FreeImageErrorHandler);
			s_FreeImageInitialized = true;
		}

		if (s_TifPreferLibTiff && fileName.find(".tif") != std::string::npos)
		{
			return copyFromFileIntoImageViaLibTiff(fileName, srgb, numFaces);
		}

		FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(fileName.c_str());

		FIBITMAP* bitmap = FreeImage_Load(fif, fileName.c_str());
		if (bitmap == nullptr)
		{
			std::cout << "could not load: " << fileName << std::endl;
			return false;
		}

		_width = FreeImage_GetWidth(bitmap);
		_height = FreeImage_GetHeight(bitmap);

		FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(bitmap);
		int bpp = FreeImage_GetBPP(bitmap);

		if (imageType == FIT_BITMAP && (bpp == 32 || bpp==24))
		{
			_format = VK_FORMAT_B8G8R8A8_UNORM;
		}
		else
		{
			std::cout << "unknown format!!" << fileName << std::endl;
			return false;
		}

		_numMipMapLevels = 1;

		const int imageNumChannels = bpp / 8;

		int srcSize = _width * _height * imageNumChannels;

		std::vector<std::uint8_t> pixelData;
		pixelData.resize(srcSize);

		std::uint32_t pitch = FreeImage_GetPitch(bitmap);
		std::uint32_t line = FreeImage_GetLine(bitmap);

		std::uint32_t rowSize = _width * imageNumChannels;
		if (rowSize != pitch)
		{
			std::uint8_t* dstPixels = pixelData.data();
			for (int y = 0; y < _height; ++y)
			{
				std::uint8_t* tmp = FreeImage_GetScanLine(bitmap, y);
				memcpy(dstPixels, tmp, rowSize);
				dstPixels += rowSize;
			}
		}
		else
		{
			memcpy(pixelData.data(), FreeImage_GetBits(bitmap), pixelData.size());
		}

		if (imageNumChannels == 3)
		{
			std::vector<std::uint8_t> pixelData4Channels;
			pixelData4Channels.resize(_width * _height * 4);

			for (int i = 0; i < _width * _height; ++i)
			{
				pixelData4Channels[4 * i + 0] = pixelData[3 * i + 0];
				pixelData4Channels[4 * i + 1] = pixelData[3 * i + 1];
				pixelData4Channels[4 * i + 2] = pixelData[3 * i + 2];
				pixelData4Channels[4 * i + 3] = 255;
			}
			pixelData.swap(pixelData4Channels);
		}

		std::vector<int> dataOffsets = { 0 };
		copyFromRawDataIntoImage(pixelData.data(), pixelData.size(), dataOffsets, 1);

		FreeImage_Unload(bitmap);

		return true;
	}

	bool Image::copyFromFileIntoImageKtx(const std::string& fileName, bool srgb, uint32_t numFaces)
	{
		std::ifstream ifs(fileName.c_str());
		if (ifs.fail())
		{
			return false;
		}

		ktxTexture* ktxTexture = nullptr;
		ktxResult result = ktxTexture_CreateFromNamedFile(fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

		GEN_ASSERT(result == KTX_SUCCESS);
		if (result != KTX_SUCCESS)
		{
			return false;
		}

		ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		_numMipMapLevels = ktxTexture->numLevels;
		_width = ktxTexture->baseWidth;
		_height = ktxTexture->baseHeight;
		_format = toVulkanFormat(ktxTexture->glInternalformat, srgb);

		std::vector<int> dataOffsets;
		for (uint32_t face = 0; face < numFaces; ++face)
		{
			for (uint32_t i = 0; i < (uint32_t)_numMipMapLevels; ++i)
			{
				ktx_size_t offset = 0;
				KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, face, &offset);
				if (ret != KTX_SUCCESS)
				{
					std::cout << __FUNCTION__ << "ret != KTX_SUCCESS" << std::endl;
				}
				dataOffsets.push_back((int)offset);
			}
		}

		GEN_ASSERT(_numMipMapLevels == dataOffsets.size() / numFaces);
		if (_numMipMapLevels != dataOffsets.size() / numFaces)
		{
			std::cout << __FUNCTION__ << "numMipMapLevels != dataOffsets.size()/numFaces" << std::endl;
		}

		copyFromRawDataIntoImage(ktxTextureData, ktxTextureSize, dataOffsets, numFaces);

		ktxTexture_Destroy(ktxTexture);

		return true;
	}

	bool Image::copyFromFileIntoImage(const std::string& fileName, bool srgb, uint32_t numFaces)
	{
		bool ok = false;
		if (fileName.find(".ktx") != std::string::npos)
		{
			 ok = copyFromFileIntoImageKtx(fileName, srgb, numFaces);
		}
		else
		{
			ok = copyFromFileIntoImageViaFreeImage(fileName, srgb, numFaces);
		}

		debugmarker::setName(_device->vulkanDevice(), _image, fileName.c_str());

		return ok;
	}

	static void roundUptoOne(int32_t& val)
	{
		if (val == 0)
		{
			val = 1;
		}
	}

	void Image::generateMipMaps(void)
	{
		// Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
		VkCommandBuffer commandBuffer = _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		for (uint32_t i = 1; i < (uint32_t)_numMipMapLevels; i++) {
			VkImageBlit imageBlit{};

			// This is the previous level, which is the source for the next level
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcOffsets[1].x = int32_t(_width >> (i - 1));
			imageBlit.srcOffsets[1].y = int32_t(_height >> (i - 1));
			imageBlit.srcOffsets[1].z = 1;

			roundUptoOne(imageBlit.srcOffsets[1].x);
			roundUptoOne(imageBlit.srcOffsets[1].y);

			// This is the destination level
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstOffsets[1].x = int32_t(_width >> i);
			imageBlit.dstOffsets[1].y = int32_t(_height >> i);
			imageBlit.dstOffsets[1].z = 1;

			roundUptoOne(imageBlit.dstOffsets[1].x);
			roundUptoOne(imageBlit.dstOffsets[1].y);

			VkImageSubresourceRange mipSubRange = {};
			mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipSubRange.baseMipLevel = i;
			mipSubRange.levelCount = 1;
			mipSubRange.layerCount = 1;

			// the next level is in undefined state, because only one level was filled, and it was set as VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			transitions::setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipSubRange);

			// the source level is in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL state, because only one level was filled, and it was set as VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			vkCmdBlitImage(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

			// set this to be the source for the next blit
			transitions::setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipSubRange);
		}

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = _numMipMapLevels;
		subresourceRange.layerCount = 1;

		// transfer the whole image
		transitions::setImageLayout(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

		_device->flushCommandBuffer(commandBuffer);
	}

	bool Image::loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, int width, int height, const std::vector<int>& mipMapDataOffsets)
	{
		GEN_ASSERT(buffer);
		if (buffer == nullptr)
		{
			std::cout << __FUNCTION__ << "(buffer==nullptr)" << std::endl;
			return false;
		}
		_numMipMapLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);
		_width = width;
		_height = height;
		_format = format;

		bool ok = copyFromRawDataIntoImage(buffer, bufferSize, mipMapDataOffsets, 1);
		if (!ok)
		{
			return false;
		}

		if (_numMipMapLevels != mipMapDataOffsets.size())
		{
			std::cout << "_numMipMapLevels != mipMapDataOffsets.size(), will generate mip maps" << std::endl;
			generateMipMaps();
		}

		return ok;
	}

	bool Image::loadFromFile(const std::string& fileName, bool srgb)
	{
		bool ok = copyFromFileIntoImage(fileName, srgb, 1);
		if (!ok)
		{
			return false;
		}
		return ok;
	}

	bool Image::loadFromFileCubeMap(const std::string& fileName)
	{
		bool ok = copyFromFileIntoImage(fileName, false, 6);
		if (!ok)
		{
			return false;
		}
		return ok;
	}

	int Image::numMipMapLevels(void) const
	{
		return _numMipMapLevels;
	}

	int Image::width(void) const
	{
		return _width;
	}

	int Image::height(void) const
	{
		return _height;
	}

	bool Image::isCubeMap(void) const
	{
		return _isCubeMap;
	}

	VkFormat Image::vulkanFormat(void) const
	{
		return _format;
	}

	VkImage Image::vulkanImage(void) const
	{
		return _image;
	}

	const Device* Image::device(void) const
	{
		return _device;
	}

	VkDeviceMemory Image::vulkanDeviceMemory(void) const
	{
		return _deviceMemory;
	}

	VkDeviceSize Image::allocationSize(void) const
	{
		return _allocationSize;
	}
}

