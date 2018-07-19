#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>

#include <windows.h>

#include "ecs/ecs.h"

#define PI 3.141592f

int main()
{
  EntityManager mgr;
  EntityId eid1 = mgr.createEntity("test");
  EntityId eid2 = mgr.createEntity("test");
  EntityId eid3 = mgr.createEntity("test1");

  mgr.tick(UpdateStage{ 0.5f });
  mgr.tick();

  //Get a console handle
  HWND myconsole = GetConsoleWindow();
  //Get a handle to device context
  HDC mydc = GetDC(myconsole);

  int pixel = 0;

  //Choose any color
  COLORREF COLOR = RGB(255, 255, 255);

  //Draw pixels
  for (double i = 0; i < PI * 4; i += 0.05)
  {
    SetPixel(mydc, pixel, (int)(50 + 25 * cos(i)), COLOR);
    pixel += 1;
  }

  ReleaseDC(myconsole, mydc);
  std::cin.ignore();

  return 0;
}