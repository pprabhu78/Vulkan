/*
* Copyright (C) 2019-203 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace genesis
{
   class Device;

   class ScreenShotUtility
   {
   public:
      //! constructor
      ScreenShotUtility(Device* device);
      //! destructor
      virtual ~ScreenShotUtility();
   public:
      virtual void takeScreenShot(const std::string& fileName
         , VkImage swapChainCurrentImage, VkFormat swapChainColorFormat
         , int swapChainWidth, int swapChainHeight);
   protected:
      Device* _device = nullptr;
   };
}
