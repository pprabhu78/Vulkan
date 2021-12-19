#include "tutorial.h"


int main(int argc, char** argv)
{
   for (size_t i = 0; i < __argc; i++) { Tutorial::args.push_back(argv[i]); };
   Tutorial* myTutorial = new Tutorial();
   myTutorial->initVulkan();
   myTutorial->setupWindow();
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete(myTutorial);
   return 0;
}