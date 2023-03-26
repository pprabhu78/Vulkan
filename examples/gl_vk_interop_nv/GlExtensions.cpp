/*
* Class wrapping access to the OpenGL extension functions (ARB, NV, AMD, EXT, etc)
*
* Copyright (C) 2019-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#if GLRENDERING
#include "GlExtensions.h"

#if _WIN32
#include <windows.h>
#else
#endif

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

   // NV
   glDrawVkImageNV = (PFNGLDRAWVKIMAGENVPROC)wglGetProcAddress("glDrawVkImageNV");
   glGetVkProcAddrNV = (PFNGLGETVKPROCADDRNVPROC)wglGetProcAddress("glGetVkProcAddrNV");
   glWaitVkSemaphoreNV = (PFNGLWAITVKSEMAPHORENVPROC)wglGetProcAddress("glWaitVkSemaphoreNV");
   glSignalVkSemaphoreNV = (PFNGLSIGNALVKSEMAPHORENVPROC)wglGetProcAddress("glSignalVkSemaphoreNV");
   glSignalVkFenceNV = (PFNGLSIGNALVKFENCENVPROC)wglGetProcAddress("glSignalVkFenceNV");

   // EXT
   glDeleteSemaphoresEXT = (PFNGLDELETESEMAPHORESEXTPROC)wglGetProcAddress("glDeleteSemaphoresEXT");
   glGenSemaphoresEXT = (PFNGLGENSEMAPHORESEXTPROC)wglGetProcAddress("glGenSemaphoresEXT");
   glGetSemaphoreParameterui64vEXT = (PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC)wglGetProcAddress("glGetSemaphoreParameterui64vEXT");   
   glIsSemaphoreEXT = (PFNGLISSEMAPHOREEXTPROC)wglGetProcAddress("glIsSemaphoreEXT");
   glSemaphoreParameterui64vEXT = (PFNGLSEMAPHOREPARAMETERUI64VEXTPROC)wglGetProcAddress("glSemaphoreParameterui64vEXT");
   glSignalSemaphoreEXT = (PFNGLSIGNALSEMAPHOREEXTPROC)wglGetProcAddress("glSignalSemaphoreEXT");
   glWaitSemaphoreEXT = (PFNGLWAITSEMAPHOREEXTPROC)wglGetProcAddress("glWaitSemaphoreEXT");

#if _WIN32
   glImportSemaphoreWin32HandleEXT = (PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC)wglGetProcAddress("glImportSemaphoreWin32HandleEXT");
   glImportSemaphoreWin32NameEXT = (PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC)wglGetProcAddress("glImportSemaphoreWin32NameEXT");

   glImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)wglGetProcAddress("glImportMemoryWin32HandleEXT");
   glImportMemoryWin32NameEXT = (PFNGLIMPORTMEMORYWIN32NAMEEXTPROC)wglGetProcAddress("glImportMemoryWin32NameEXT");

   glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)wglGetProcAddress("glCreateMemoryObjectsEXT");
#else
   #error "Target platform not defined"
#endif

   _initialized = true;
}
#endif