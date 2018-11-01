#include <random>
#include <ctime>

#include <ecs/ecs.h>
#include <ecs/perf.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <script.h>
#include <scriptECS.h>

#include <scripthandle/scripthandle.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>

#include <raylib.h>

#include "update.ecs.h"

#include "cef.h"

PULL_ESC_CORE;

Camera2D camera;
int screen_width = 800;
int screen_height = 600;

bool g_enable_cef = false;

int main()
{
  std::srand(unsigned(std::time(0)));

  EntityManager::create();

  if (g_enable_cef)
    cef::init();

  InitWindow(screen_width, screen_height, "raylib [core] example - basic window");

  SetTargetFPS(60);

  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = 1.5f;

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
    doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(buffer);
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

  // TODO: Codegen for script bindings
  script::register_component<EventOnKillEnemy>("EventOnKillEnemy");
  script::register_component_property("EventOnKillEnemy", "vec2 pos", offsetof(EventOnKillEnemy, pos));

  script::register_component<EventOnWallHit>("EventOnWallHit");
  script::register_component_property("EventOnWallHit", "float d", offsetof(EventOnWallHit, d));
  script::register_component_property("EventOnWallHit", "vec2 normal", offsetof(EventOnWallHit, normal));
  script::register_component_property("EventOnWallHit", "vec2 vel", offsetof(EventOnWallHit, vel));

  script::register_component<UpdateStage>("UpdateStage");
  script::register_component_property("UpdateStage", "float dt", offsetof(UpdateStage, dt));
  script::register_component_property("UpdateStage", "float total", offsetof(UpdateStage, total));

  script::register_component<AutoMove>("AutoMove");
  script::register_component_property("AutoMove", "bool jump", offsetof(AutoMove, jump));
  script::register_component_property("AutoMove", "float time", offsetof(AutoMove, time));
  script::register_component_property("AutoMove", "float duration", offsetof(AutoMove, duration));
  script::register_component_property("AutoMove", "float length", offsetof(AutoMove, length));

  script::register_component<Jump>("Jump");
  script::register_component_property("Jump", "bool active", offsetof(Jump, active));
  script::register_component_property("Jump", "float startTime", offsetof(Jump, startTime));
  script::register_component_property("Jump", "float height", offsetof(Jump, height));
  script::register_component_property("Jump", "float duration", offsetof(Jump, duration));

  script::register_component<TimerComponent>("TimerComponent");
  script::register_component_property("TimerComponent", "float time", offsetof(TimerComponent, time));
  script::register_component_property("TimerComponent", "float period", offsetof(TimerComponent, period));

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    if (g_enable_cef)
    {
      if (IsKeyPressed(KEY_F11))
        g_mgr->sendEvent(cef::get_eid(), cef::CmdToggleDevTools{});
      else if (IsKeyPressed(KEY_F10))
        g_mgr->sendEvent(cef::get_eid(), cef::CmdToggleWebUI{});

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        g_mgr->sendEvent(cef::get_eid(), cef::EventOnClickOutside{});
    }

    double t = GetTime();
    g_mgr->tick();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    g_mgr->tick(UpdateStage{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    t = GetTime();
    BeginMode2D(camera);
    g_mgr->tick(RenderStage{});
    EndMode2D();
    const float renderDelta = (float)((GetTime() - t) * 1e3);

    BeginMode2D(camera);
    g_mgr->tick(RenderDebugStage{});
    EndMode2D();

    DrawText(FormatText("ECS time: %2.2f ms (%2.2f ms)", delta, renderDelta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawText(FormatText("FM: %d B Max: %d MB (%d kB)",
      get_frame_mem_allocated_size(),
      get_frame_mem_allocated_max_size() >> 20,
      get_frame_mem_allocated_max_size() >> 10),
      10, 70, 20, LIME);
    DrawFPS(10, 10);

    g_mgr->tick(RenderHUDStage{});

    // g_mgr->tick(RenderDebugStage{});

    EndDrawing();

    if (g_enable_cef)
      cef::update();

    clear_frame_mem();
  }

  EntityManager::release();
  script::release();

  extern void clear_textures();
  clear_textures();

  CloseWindow();

  if (g_enable_cef)
    cef::release();

  return 0;
}