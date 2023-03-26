# Vulkan <> OpenGL Interop

This sample is based off of the ray tracing sample.

Indeed, you should diff that sample with this sample to see how the set up is different for gl vs vulkan.

Only dynamic rendering is supported currently.

The OpenGL vs Vulkan rendering can be toggled using this command line: --gl

When using OpenGL to render:
- We tell the core to not use swap chain rendering so that it will render to a color image (_colorImage).
- We also set up the window to use an OpenGL context (see setupWindow).
- We render as usual using Vulkan (the image being rendered to being the _colorImage instead of the swap chain).
- In postFrame, we wait for the renderComplete semaphore (which the submit will signal after the command buffers have executed).
- We then draw this image using: glDrawVkImageNV and then signal presentComplete (which submit waits on for the next frame)

There is a bug in either the Vulkan driver or this sample that causes device creation to sometimes fail. I have a suspicion that this may be due to even the inclusion of glew.

This sample is also wildly susceptible to crashes. I suspect this is because the nvidia extensions for inter-op are ad-hoc implemented

Also, the nvidia extensions for synchronization and drawing image don't support the validation layer. If you use it, it will randomly crash.
