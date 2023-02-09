/*
* Class wrapping access to the OpenGL extension functions (ARB, NV, AMD, EXT, etc)
*
* Copyright (C) 2019-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "GlExtensions.h"

#if _WIN32
#include <windows.h>
#else
#endif

namespace genesis
{
   glExtensions::glExtensions()
   {
      // no op
   }

   glExtensions::~glExtensions()
   {
      // no op
   }

   void glExtensions::initialize()
   {
      if (_initialized)
      {
         return;
      }

      glDrawVkImageNV = (PFNGLDRAWVKIMAGENVPROC)wglGetProcAddress("glDrawVkImageNV");
      glGetVkProcAddrNV = (PFNGLGETVKPROCADDRNVPROC)wglGetProcAddress("glGetVkProcAddrNV");
      glWaitVkSemaphoreNV = (PFNGLWAITVKSEMAPHORENVPROC)wglGetProcAddress("glWaitVkSemaphoreNV");
      glSignalVkSemaphoreNV = (PFNGLSIGNALVKSEMAPHORENVPROC)wglGetProcAddress("glSignalVkSemaphoreNV");
      glSignalVkFenceNV = (PFNGLSIGNALVKFENCENVPROC)wglGetProcAddress("glSignalVkFenceNV");

      _initialized = true;
   }
}