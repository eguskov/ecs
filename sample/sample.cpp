#include <random>
#include <ctime>

#include <ecs/ecs.h>
#include <ecs/perf.h>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "update.ecs.h"

#include <script.h>
#include <scriptECS.h>

#include <scripthandle/scripthandle.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>

PULL_ESC_CORE;

template <size_t N>
void call(const eastl::array<int, N> &offets, uint8_t** compontes)
{
  for (int i = 0; i < N; ++i)
  {
    int offset = offsets[i];
    void *mem = compontes[i][offset];
  }
}

Camera2D camera;
int screen_width = 800;
int screen_height = 450;

template <size_t... I>
void testFunc(eastl::index_sequence<I...>);

template <>
void testFunc<>(eastl::index_sequence<0, 1, 2>)
{

}

using SUpdateStage = script::ScriptHelperDesc<UpdateStage, 1>;
using SEventOnKillEnemy = script::ScriptHelperDesc<EventOnKillEnemy, 1>;
using SEventOnWallHit = script::ScriptHelperDesc<EventOnWallHit, 1>;

int main()
{
  testFunc<0, 1, 2>(eastl::make_index_sequence<3>{});

  std::srand(unsigned(std::time(0)));

  InitWindow(screen_width, screen_height, "raylib [core] example - basic window");

  SetTargetFPS(60);

  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = 1.5f;

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
      g_mgr->createEntity(ent["$template"].GetString(), ent["$components"]);
    }
  }

  script::init();

  script::register_component<SEventOnKillEnemy>("EventOnKillEnemy", find_comp("EventOnKillEnemy"));

  script::register_component<SEventOnWallHit>("EventOnWallHit", find_comp("EventOnWallHit"));
  script::register_component_property("EventOnWallHit", "vec2 normal", offsetof(EventOnWallHit, normal));

  script::register_component<SUpdateStage>("UpdateStage", find_comp("UpdateStage"));
  script::register_component_property("UpdateStage", "float dt", offsetof(UpdateStage, dt));
  script::register_component_property("UpdateStage", "float total", offsetof(UpdateStage, total));

  //script::ScriptECS scriptECS;
  //scriptECS.build("script.as");

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    double t = GetTime();
    g_mgr->tick();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    g_mgr->tick(UpdateStage{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode2D(camera);
    g_mgr->tick(RenderStage{});
    EndMode2D();

    DrawText(FormatText("ECS time: %f ms", delta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawFPS(10, 10);

    g_mgr->tick(RenderHUDStage{});

    EndDrawing();
  }

  EntityManager::release();
  script::release();

  CloseWindow();

  return 0;
}