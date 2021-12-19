#include "tutorial_raytracing.h"

int main(int argc, char** argv)
{
   for (size_t i = 0; i < __argc; i++) { TutorialRayTracing::args.push_back(argv[i]); };
   TutorialRayTracing* myTutorial = new TutorialRayTracing();
   myTutorial->initVulkan();
   myTutorial->setupWindow();
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete myTutorial;
return 0;
}