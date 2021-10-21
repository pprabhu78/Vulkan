#include "tutorial.h"

Tutorial* myTutorial;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (myTutorial != NULL)
   {
      myTutorial->handleMessages(hWnd, uMsg, wParam, lParam);
   }
   return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
   for (size_t i = 0; i < __argc; i++) { Tutorial::args.push_back(__argv[i]); };
   myTutorial = new Tutorial();
   myTutorial->initVulkan();
   myTutorial->setupWindow(hInstance, WndProc);
   myTutorial->prepare();
   myTutorial->renderLoop();
   delete(myTutorial);
   return 0;
}