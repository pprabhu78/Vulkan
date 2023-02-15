#include "gl_vk_interop.h"

int main(int argc, char** argv)
{
   for (size_t i = 0; i < __argc; i++) { RayTracing::_args.push_back(argv[i]); };
   RayTracing* myTutorial = new RayTracing();
   myTutorial->initVulkan();
   myTutorial->setupWindow();
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete myTutorial;
return 0;
}