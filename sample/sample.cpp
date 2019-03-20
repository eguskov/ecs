#include <random>
#include <ctime>

#include <ecs/ecs.h>
#include <ecs/hash.h>
#include <ecs/perf.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <script.h>
#include <script-ecs.h>

#include <scripthandle/scripthandle.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>

#include <raylib.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>

#include "update.h"

#include "cef.h"
#include "webui.h"

PULL_ESC_CORE;

Camera2D camera;
int screen_width = 800;
int screen_height = 600;

bool g_enable_cef = false;

std::string get_hashed_string(const HashedString &s)
{
  return std::string(s.str);
}

void script_init()
{
  script::init();

  // TODO: Codegen for script bindings
  script::register_component<EventOnKillEnemy>("EventOnKillEnemy");
  script::register_component_property("EventOnKillEnemy", "vec2 pos", offsetof(EventOnKillEnemy, pos));

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

  script::register_component<HashedString>("HashedString");
  script::register_component_property("HashedString", "uint hash", offsetof(HashedString, hash));
  script::register_component_function("HashedString", "string str() const", asFUNCTION(get_hashed_string));
}

void print_document(const JFrameDocument &doc)
{
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  std::cout << buffer.GetString() << std::endl;
}

bool process_script_command(int argc, char *argv[])
{
  if (argc < 2)
    return false;

  if (::strcmp(argv[1], "--compile") == 0 && argc > 2 && ::strlen(argv[2]) > 0)
  {
    JFrameDocument res;
    res.SetObject();

    script::compile_module(argv[2], argv[2], res);

    print_document(res);

    return true;
  }
  else if (::strcmp(argv[1], "--inspect") == 0 && argc > 2 && ::strlen(argv[2]) > 0)
  {
    JFrameDocument res;
    res.SetObject();

    script::inspect_module(argv[2], argv[2], res);
    script::save_all_bindings_to_document(res);

    print_document(res);

    return true;
  }

  return false;
}

int main(int argc, char *argv[])
{
  log4cplus::initialize();

  log4cplus::BasicConfigurator config;
  config.configure();

  log4cplus::Logger logger = log4cplus::Logger::getRoot();
  LOG4CPLUS_WARN_FMT(logger, "%s, %s!", "Hello", "World");

  stacktrace::init();

  std::srand(unsigned(std::time(0)));

  script_init();

  if (process_script_command(argc, argv))
  {
    script::release();
    return 0;
  }

  EntityManager::create();

  if (g_enable_cef)
    cef::init();

  webui::init("127.0.0.1:10112");

  InitWindow(screen_width, screen_height, "ECS Sample");

  SetTargetFPS(60);

  const float hw = 0.5f * screen_width;
  const float hh = 0.5f * screen_height;
  camera.target = Vector2{ hw, hh };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  FILE *file = nullptr;
  ::fopen_s(&file, "data/entities.json", "rb");
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

    JFrameDocument doc;
    doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(buffer);
    delete[] buffer;

    ASSERT(doc.HasMember("$entities"));
    ASSERT(doc["$entities"].IsArray());
    for (int i = 0; i < (int)doc["$entities"].Size(); ++i)
    {
      const JFrameValue &ent = doc["$entities"][i];
      g_mgr->createEntity(ent["$template"].GetString(), ent["$components"]);
    }
  }

  double nextResetMinMax = 0.0;

  float minDelta = 1000.f;
  float maxDelta = 0.f;

  float minRenderDelta = 1000.f;

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

    if (totalTime > nextResetMinMax)
    {
      nextResetMinMax = totalTime + 1.0;

      minDelta = 1000.f;
      maxDelta = 0.f;

      minRenderDelta = 1000.f;
    }

    double t = GetTime();
    g_mgr->tick();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    g_mgr->tick(UpdateStage{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    minDelta = eastl::min(minDelta, delta);
    maxDelta = eastl::max(maxDelta, delta);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    t = GetTime();
    BeginMode2D(camera);
    g_mgr->tick(RenderStage{});
    EndMode2D();
    const float renderDelta = (float)((GetTime() - t) * 1e3);

    minRenderDelta = eastl::min(minRenderDelta, renderDelta);

    #ifdef _DEBUG
    BeginMode2D(camera);
    g_mgr->tick(RenderDebugStage{});
    EndMode2D();
    #endif

    DrawText(FormatText("ECS time: %2.2f ms (%2.2f ms)", /* delta */minDelta, /* renderDelta */ minRenderDelta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawText(FormatText("FM: %d B Max: %d MB (%d kB)",
      get_frame_mem_allocated_size(),
      get_frame_mem_allocated_max_size() >> 20,
      get_frame_mem_allocated_max_size() >> 10),
      10, 70, 20, LIME);
    DrawFPS(10, 10);

    g_mgr->tick(RenderHUDStage{});

    EndDrawing();

    if (g_enable_cef)
      cef::update();

    webui::update();

    clear_frame_mem();
  }

  webui::release();

  EntityManager::release();
  script::release();

  extern void clear_textures();
  clear_textures();

  CloseWindow();

  if (g_enable_cef)
    cef::release();

  stacktrace::release();

  log4cplus::deinitialize();

  return 0;
}