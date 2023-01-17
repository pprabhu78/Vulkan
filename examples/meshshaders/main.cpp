#include "meshshaders.h"

int main(int argc, char** argv)
{
   for (size_t i = 0; i < __argc; i++) { MeshShaders::args.push_back(argv[i]); };
   MeshShaders* myTutorial = new MeshShaders();
   myTutorial->initVulkan();
   myTutorial->setupWindow();
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete myTutorial;
return 0;
}