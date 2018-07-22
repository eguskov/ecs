#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>
#include <random>
#include <ctime>

#include <windows.h>

#include "ecs/ecs.h"

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/velocity.component.h"

#include "systems/update.h"

#define PI 3.141592f

void loop()
{
  DWORD tick = ::GetTickCount();
  while (true)
  {
    DWORD dt = ::GetTickCount() - tick;
    tick = ::GetTickCount();

    if (dt == 0)
      dt = 1;

    g_mgr->tick(UpdateStage{ float(dt) / 1000.f });
    g_mgr->tick(RenderStage{});
    g_mgr->tick();

    Sleep(1);
  }
}

int main()
{
  std::srand(unsigned(std::time(0)));

  EntityManager::init();

  FILE *file = nullptr;
  ::fopen_s(&file, "entities.json", "rb");
  if (file)
  {
    size_t sz = ::ftell(file);
    ::fseek(file, 0, SEEK_END);
    sz = ::ftell(file) - sz;
    ::fseek(file, 0, SEEK_SET);

    char *buffer = new char[sz + 1];
    buffer[sz] = '\0';
    ::fread(buffer, 1, sz, file);
    ::fclose(file);

    JDocument doc;
    doc.Parse(buffer);
    delete[] buffer;

    assert(doc.HasMember("$entities"));
    assert(doc["$entities"].IsArray());
    for (int i = 0; i < (int)doc["$entities"].Size(); ++i)
    {
      const JValue &ent = doc["$entities"][i];
      g_mgr->createEntitySync(ent["$template"].GetString(), ent["$components"]);
    }
  }

  std::vector<EntityId> eids;
  g_mgr->query(eids, { { "velocity", "vel" },{ "position", "pos" } });

  for (EntityId eid : eids)
  {
    const auto &vel = g_mgr->getComponent<VelocityComponent>(eid, "vel");
    continue;
  }

  //for (int i = 0; i < 10; ++i)
  //  g_mgr->createEntity("test");

  //EntityId eid1 = g_mgr->createEntity("test");
  //EntityId eid2 = g_mgr->createEntity("test");
  //EntityId eid3 = g_mgr->createEntity("test1");

  loop();

  /*g_mgr->tick(UpdateStage{ 0.5f });

  g_mgr->sendEvent(eid2, EventOnTest{ 5.f, 10.f });
  g_mgr->sendEvent(eid1, EventOnAnotherTest{ 5.f, 10.f, 25.f });

  g_mgr->tick();

  g_mgr->sendEvent(eid1, EventOnAnotherTest{ 5.f, 10.f, 25.f });
  g_mgr->sendEvent(eid2, EventOnTest{ 5.f, 10.f });

  g_mgr->tick();*/

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