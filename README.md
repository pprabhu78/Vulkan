


# Path tracing and global illumination using Vulkan

![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-4-17_155829.png)
![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-4-17_161326.png)
![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-4-17_161518.png)
![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-4-17_161639.png)

Initially, I started this project to learn Vulkan and wrote a sandbox engine for trying out things in Vulkan.

I started by forking Sascha Willems' excellent resource: https://github.com/SaschaWillems/Vulkan. I would recommend this as the first stop, bar none, for anyone trying to learn Vulkan. 

As I got more familiar with Vulkan, I also decided to get back into Ray Tracing and Global Illumination, and the project diverged. 

I used these as reference:  
 - Reference path tracer from Jakub Boksansky and supplemental literature:  
  -https://github.com/boksajak/referencePT  
  -https://boksajak.github.io/blog/BRDF
   
 - And ofcourse, the classic, 'Physically Based Rendering'  
   -https://www.pbr-book.org/
   
 - I also used nvidia's samples for some of the nitty gritty implementation details as reference (specific to ray tracing and vulkan in general):  
  -https://github.com/nvpro-samples/vk_raytrace  
  -https://github.com/nvpro-samples/
    
This project currently has fundamentally just 2 parts:  
-The 'genesis' engine, which is the set of classes encapsulating core Vulkan functionality like buffers, textures, images, shaders, gltf and so on.  
-The ray tracing application that uses this engine.

The sample continues to increase in functionality. Currently it supports:
 - Diffuse and Specular brdfs specified the PBR/gltf way (metalness, roughness, etc.) and some support for [transmission](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_transmission).
 - World building specification (which leads itself to indirect rendering as well):  
  -There can be multiple models (a 'model' is typically a single gltf file, but in theory it can come from anything or even runtime created)  
  -There can be multiple instances of such models  
  -Multiple instances of multiple models go into cells  
  -There can be multiple cells

 - You can switch between ray tracing and rasterization. Rasterization uses indirect rendering.

 - Everything is bindless. I used Nvidia's buffer reference extension: https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_buffer_reference.txt   
  -Vertex and index buffers for multiple models go into multiple buffers  
  -Material (properties, textures) for multiple models go into a buffer of materials  
  -There is a buffer of instances (corresponding to instances of models in a cell)

## Prerequisites
You need an [RTX](https://www.nvidia.com/en-us/geforce/graphics-cards/30-series/) card with the latest drivers and also need the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)

## Cloning
This repository contains submodules for external dependencies, so when doing a fresh clone you need to clone recursively:

```
git clone --recursive https://github.com/pprabhu78/Vulkan
```

Existing repositories can be updated manually:

```
git submodule init
git submodule update
```

## Building

The repository contains everything required to compile and build the examples on <img src="./images/windowslogo.png" alt="" height="22px" valign="bottom"> Windows, <img src="./images/linuxlogo.png" alt="" height="24px" valign="bottom"> Linux.

On windows, you generate the solution using: cmake -G "Visual Studio 17 2022"

I have not compiled, tested on linux.

## Assets
There are a handful of models in the data/models folder. You can use other gltf models. For example:
https://github.com/KhronosGroup/glTF-Sample-Models

You can create and/or export glTF in [Blender](www.blender.org) (or any other modeling software)

You can drag and drop .glb or .gltf files onto the application. 

## Running

Once built, the application can be run from the bin directory. 
`
## Credits and Attributions
Huge thanks to all the entities mentioned in above for everything.
See [CREDITS.md](CREDITS.md) for additional credits and attributions.

PS: The name of the engine is a nod to the 'genesis effect' from [Star Trek II: The Wrath of Khan](https://en.wikipedia.org/wiki/Star_Trek_II:_The_Wrath_of_Khan)
![enter image description here](https://static.wikia.nocookie.net/memoryalpha/images/e/e1/Genesis_effect.jpg/revision/latest/scale-to-width-down/1000?cb=20100624221212&path-prefix=en)
which was the [first fully textured CGI effect in film.](https://memory-alpha.fandom.com/wiki/Pixar) 
