# Vulkan <> OpenGL Interop

This sample is based off of the gl_vk_interop_nv sample.

This sample uses only Vulkan and OpenGL extensions.

It does not use any Nvidia specific extensions.

Only dynamic rendering is supported currently.

The OpenGL vs Vulkan rendering can be toggled using this command line: --gl

During initialization:
 - Create OpenGL side semaphores that mirror the core _presentComplete & _renderComplete semaphores.
 - Create OpenGL side texture that mirrors vulkan side _colorImage.

To Render:
- We tell the core to not use swap chain rendering so that it will render to a color image (_colorImageGlSide).
- We also set up the window to use an OpenGL context (see setupWindow).
- We render as usual using Vulkan (the image being rendered to being the _colorImageGlSide instead of the swap chain).
- In postFrame, we wait for the _renderCompleteGlSide semaphore (which the submit will signal after the command buffers have executed).
- We then draw this _colorImageGlSide using full screen quad using pure OpenGL rendering.
- We then signal _presentCompleteGlSide (which submit waits on for the next frame).
