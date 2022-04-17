

# Vulkan sandbox engine for ray tracing, etc.

![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-3-13_55730.png)
![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/screenshots/2022-3-13_55826.png)
![enter image description here](https://github.com/pprabhu78/Vulkan/blob/master/autotest/cornell_raytrace.png)

Sandbox engine for trying out things in Vulkan.

Initially, I started this project to learn Vulkan. 

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
-The 'genesis' engine, which is the classes encapsulating core Vulkan functionality like buffers, textures, images, shaders, gltf and so on.  
-The ray tracing sample that uses this engine.

PS: The name of the engine is a node to the 'genesis effect' from [Star Trek II: The Wrath of Khan](https://en.wikipedia.org/wiki/Star_Trek_II:_The_Wrath_of_Khan)
![enter image description here](https://memory-alpha.fandom.com/wiki/Pixar?file=Genesis_effect.jpg)
which was the [first fully textured CGI effect in film.](https://memory-alpha.fandom.com/wiki/Pixar) 

The sample continues to increase in functionality. Currently it supports:  

 - Diffuse and Specular brdfs specified the PBR/gltf way (metalness, roughness, etc)
 
 - World building specification (which leads itself to indirect rendering as well):  
  -There can be multiple models (a 'model' is typically a single gltf file, but in theory it can come from anything or even runttime created)  
  -There can be multiple instances of such models  
  -Multiple instances of multiple models go into cells  
  -There can be multiple cells

 - You can switch between ray tracing and rasterization. Rasterization uses indirect rendering.

 - Everything is bindless. I used Nvidia's buffer reference extension: https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_buffer_reference.txt   
  -Vertex and index buffers for multiple models go into multiple buffers  
  -Material (properties, textures) for multiple models go into a buffer of materials  
  -There is a buffer of instances (corresponding to instances of models in a cell)

## Cloning
This repository contains submodules for external dependencies, so when doing a fresh clone you need to clone recursively:

```
git clone --recursive https://github.com/SaschaWillems/Vulkan.git
```

Existing repositories can be updated manually:

```
git submodule init
git submodule update
```

## Assets
Many examples require assets from the asset pack that is not part of this repository due to file size. A python script is included to download the asset pack that. Run

    python download_assets.py

from the root of the repository after cloning or see [this](data/README.md) for manual download.

## Building

The repository contains everything required to compile and build the examples on <img src="./images/windowslogo.png" alt="" height="22px" valign="bottom"> Windows, <img src="./images/linuxlogo.png" alt="" height="24px" valign="bottom"> Linux, <img src="./images/androidlogo.png" alt="" height="24px" valign="bottom"> Android, <img src="./images/applelogo.png" alt="" valign="bottom" height="24px"> iOS and macOS (using MoltenVK) using a C++ compiler that supports C++11.

See [BUILD.md](BUILD.md) for details on how to build for the different platforms.

## Running

Once built, examples can be run from the bin directory. The list of available command line options can be brought up with `--help`:
`
## Credits and Attributions
Huge thanks to all the entities mentioned in above for everything.
See [CREDITS.md](CREDITS.md) for additional credits and attributions.
