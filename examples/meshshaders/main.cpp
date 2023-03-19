#include "meshshaders.h"

int main(int argc, char** argv)
{
   for (size_t i = 0; i < __argc; i++) { MeshShaders::_args.push_back(argv[i]); };
   MeshShaders* myTutorial = new MeshShaders();
   bool ok = myTutorial->initVulkan();
   if (!ok)
   {
      std::cout << "Initialization not succesful, quitting" << std::endl;
      delete myTutorial;
      return 0;
   }
   myTutorial->setupWindow();
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete myTutorial;
   return 0;
}