/*
* Class wrapping access to the OpenGL extension functions (ARB, NV, AMD, EXT, etc)
*
* Copyright (C) 2019-2023 by P. Prabhu/PSquare Interactive, LLC. - https://github.com/pprabhu78
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#if GLRENDERING
#include <GL/glew.h>
class glExtensions
{
public:
   glExtensions();
   virtual ~glExtensions();
public:
   virtual void initialize();
public:
   // NV
   PFNGLDRAWVKIMAGENVPROC glDrawVkImageNV = nullptr;
   PFNGLGETVKPROCADDRNVPROC glGetVkProcAddrNV = nullptr;
   PFNGLWAITVKSEMAPHORENVPROC glWaitVkSemaphoreNV = nullptr;
   PFNGLSIGNALVKSEMAPHORENVPROC glSignalVkSemaphoreNV = nullptr;
   PFNGLSIGNALVKFENCENVPROC glSignalVkFenceNV = nullptr;

   // EXT
   PFNGLDELETESEMAPHORESEXTPROC glDeleteSemaphoresEXT = nullptr;
   PFNGLGENSEMAPHORESEXTPROC glGenSemaphoresEXT = nullptr;
   PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC glGetSemaphoreParameterui64vEXT = nullptr;
   PFNGLISSEMAPHOREEXTPROC glIsSemaphoreEXT = nullptr;
   PFNGLSEMAPHOREPARAMETERUI64VEXTPROC glSemaphoreParameterui64vEXT = nullptr;
   PFNGLSIGNALSEMAPHOREEXTPROC glSignalSemaphoreEXT = nullptr;
   PFNGLWAITSEMAPHOREEXTPROC glWaitSemaphoreEXT = nullptr;

#if _WIN32
   PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC glImportSemaphoreWin32HandleEXT = nullptr;
   PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC glImportSemaphoreWin32NameEXT = nullptr;

   PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT = nullptr;
   PFNGLIMPORTMEMORYWIN32NAMEEXTPROC glImportMemoryWin32NameEXT = nullptr;

   PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT = nullptr;
#else
   #error "Target platform not defined"
#endif

protected:
   bool _initialized = false;
};
#endif