#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace genesis
{
   class Device;

   class ScreenShotUtility
   {
   public:
      ScreenShotUtility(Device* device);

      virtual ~ScreenShotUtility();

   public:
      virtual void takeScreenShot(const std::string& fileName
         , VkImage swapChainCurrentImage, VkFormat swapChainColorFormat
         , int swapChainWidth, int swapChainHeight);
   protected:
      Device* _device;
   };
}
