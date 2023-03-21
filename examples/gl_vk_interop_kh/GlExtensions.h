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
   PFNGLDRAWVKIMAGENVPROC glDrawVkImageNV = nullptr;
   PFNGLGETVKPROCADDRNVPROC glGetVkProcAddrNV = nullptr;
   PFNGLWAITVKSEMAPHORENVPROC glWaitVkSemaphoreNV = nullptr;
   PFNGLSIGNALVKSEMAPHORENVPROC glSignalVkSemaphoreNV = nullptr;
   PFNGLSIGNALVKFENCENVPROC glSignalVkFenceNV = nullptr;
protected:
   bool _initialized = false;
};
#endif