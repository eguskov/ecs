#include "stdafx.h"

#include <random>
#include <ctime>

#include "ecs/ecs.h"

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/velocity.component.h"

#include "systems/update.h"

template <size_t N>
void call(const eastl::array<int, N> &offets, uint8_t** compontes)
{
  for (int i = 0; i < N; ++i)
  {
    int offset = offsets[i];
    void *mem = compontes[i][offset];
  }
}

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
      g_mgr->createEntitySyncSoA(ent["$template"].GetString(), ent["$components"]);
    }
  }

  g_mgr->query<void(const VelocityComponent&)>([](const VelocityComponent &vel)
  {
  },
  { "vel" });

  InitWindow(screen_width, screen_height, "raylib [core] example - basic window");

  SetTargetFPS(60);

  while (!WindowShouldClose())
  {
    double t = GetTime();
    g_mgr->tick();
    g_mgr->tickSoA(UpdateStage{ GetFrameTime() });
    float delta = (float)((GetTime() - t) * 1e3);

    BeginDrawing();

    ClearBackground(RAYWHITE);

    g_mgr->tickSoA(RenderStage{});

    DrawText(FormatText("ECS time: %f ms", delta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entitiesSoA.size()), 10, 50, 20, LIME);
    DrawFPS(10, 10);

    EndDrawing();
  }
  
  CloseWindow();

  return 0;
}