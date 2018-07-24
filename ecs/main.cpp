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

#include "ecs/ecs.h"

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/velocity.component.h"

#include "systems/update.h"

int screen_width = 800;
int screen_height = 450;

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

  InitWindow(screen_width, screen_height, "raylib [core] example - basic window");

  SetTargetFPS(60);

  while (!WindowShouldClose())
  {
    double t = GetTime();
    g_mgr->tick();
    g_mgr->tick(UpdateStage{ GetFrameTime() });

    BeginDrawing();

    ClearBackground(RAYWHITE);

    g_mgr->tick(RenderStage{});

    DrawText(FormatText("ECS time: %f ms", (GetTime() - t) * 1e3), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawFPS(10, 10);

    EndDrawing();
  }
  
  CloseWindow();

  return 0;
}