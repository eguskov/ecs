#include <random>
#include <ctime>

#include <sys/stat.h>

#include <ecs/ecs.h>
#include <ecs/hash.h>
#include <ecs/perf.h>
#include <ecs/jobmanager.h>
#include <ecs/autoBind.h>

#include <raylib.h>

#pragma warning(push)
#pragma warning(disable: 4838)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#pragma warning(pop)

#include <daScript/daScript.h>
#include <daScript/simulate/fs_file_info.h>
#include <daScript/misc/sysos.h>

#include "update.h"
#include "boids.h"

PULL_ESC_CORE;

Camera2D camera;
int screen_width = 800;
int screen_height = 600;

std::string get_hashed_string(const HashedString &s)
{
  return std::string(s.str);
}

#include <EASTL/unique_ptr.h>

struct AnimModule final : public das::Module
{
  AnimModule() : das::Module("anim")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("anim"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(AnimModule);

struct RenderModule final : public das::Module
{
  RenderModule() : das::Module("render")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("render"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(RenderModule);

struct PhysModule final : public das::Module
{
  PhysModule() : das::Module("phys")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("phys"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(PhysModule);

struct SampleModule final : public das::Module
{
  SampleModule() : das::Module("sample")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("sample"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(SampleModule);

using DasContextPtr = eastl::unique_ptr<EcsContext>;
DasContextPtr dasCtx;

static constexpr char *dasRoot = "../libs/daScript";
static constexpr char *dasProject = "scripts/project.das_project";
static constexpr char *dasInitScript = "scripts/sample.das";
static constexpr char *dasMainScript = "scripts/main.das";
static eastl::hash_map<eastl::string, SystemId> dasMainSystems;
static eastl::vector<QueryId> dasMainQueries;
static time_t dasMainLastModified = 0;
static bool isDasInitScriptLoaded = false;

template <typename TFileAccess>
static bool compile_and_simulate_script(DasContextPtr &ctx, const char *path, das::ModuleGroup &libGroup, const TFileAccess &file_access)
{
  das::TextPrinter tout;
  auto program = das::compileDaScript(path, file_access, tout, libGroup);
  if (!program)
    return false;
  
  if (program->failed())
  {
    for (auto &err : program->errors)
      tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
    std::cout.flush();
    return false;
  }

  DasContextPtr tmpCtx = eastl::make_unique<EcsContext>(program->getContextStackSize());
    
  if (!program->simulate(*tmpCtx, tout))
  {
    for (auto &err : program->errors)
      tout << das::reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
    std::cout.flush();
    return false;
  }

  ctx = eastl::move(tmpCtx);

  return true;
}

static bool reload_scripts()
{
  if (!isDasInitScriptLoaded)
  {
    auto fAccess = das::make_smart<das::FsFileAccess>(dasProject, das::make_smart<das::FsFileAccess>());
    DasContextPtr ctx;
    das::ModuleGroup libGroup;
    isDasInitScriptLoaded = compile_and_simulate_script(ctx, dasInitScript, libGroup, fAccess);
  }

  if (!isDasInitScriptLoaded)
    return false;

  struct stat st;
  if (::stat(dasMainScript, &st) == 0)
  {
    if (st.st_mtime == dasMainLastModified)
      return true;
    dasMainLastModified = st.st_mtime;
  }
  else
  {
    ASSERT(false);
    return false;
  }

  auto fAccess = das::make_smart<das::FsFileAccess>(dasProject, das::make_smart<das::FsFileAccess>());
  das::ModuleGroup libGroup;
  libGroup.setUserData(new EcsModuleGroupData);
  const bool res = compile_and_simulate_script(dasCtx, dasMainScript, libGroup, fAccess);
  if (res)
  {
    for (auto kv : dasMainSystems)
      g_mgr->deleteSystem(kv.second);

    for (QueryId qid : dasMainQueries)
      g_mgr->deleteQuery(qid);

    dasMainSystems.clear();
    dasMainQueries.clear();

    auto *moduleData = ((EcsModuleGroupData*)libGroup.getUserData("ecs"));
    dasMainQueries = moduleData->dasQueries;

    for (auto &s : moduleData->unresolvedSystems)
    {
      auto res = dasMainSystems.insert(s.first);

      SystemId sid = g_mgr->createSystem(s.first.c_str(), s.second.systemDesc.release(), &s.second.queryDesc);
      res.first->second = sid;

      g_mgr->queries[g_mgr->systems[sid.index].queryId.index].userData = eastl::move(s.second.queryData);
    }

    for (auto kv : dasMainSystems)
    {
      SystemId sid = kv.second;
      DasQueryData *queryData = (DasQueryData*)g_mgr->queries[g_mgr->systems[sid.index].queryId.index].userData.get();
      queryData->ctx = dasCtx.get();
      queryData->fn  = dasCtx->findFunction(kv.first.c_str());
    }
  }
  return res;
}

bool init_sample()
{
  extern int das_def_tab_size;
  das_def_tab_size = 2;

  // TODO: Pass from command line
  das::setDasRoot(dasRoot);

  NEED_MODULE(Module_BuiltIn);
  NEED_MODULE(Module_Strings);
  NEED_MODULE(Module_Math);
  NEED_MODULE(Module_Rtti);
  NEED_MODULE(Module_Ast);
  NEED_MODULE(Module_FIO);
  NEED_MODULE(ECSModule);
  NEED_MODULE(AnimModule);
  NEED_MODULE(RenderModule);
  NEED_MODULE(PhysModule);
  NEED_MODULE(SampleModule);

  return reload_scripts();
}

int main(int argc, char *argv[])
{
  stacktrace::init();

  std::srand(unsigned(std::time(0)));

  // TODO: Update queries after templates registratina has been done
  ecs::init();

  init_sample();

  // SetConfigFlags(FLAG_WINDOW_UNDECORATED);
  InitWindow(screen_width, screen_height, "ECS Sample");

  SetTargetFPS(60);

  const float hw = 0.5f * screen_width;
  const float hh = 0.5f * screen_height;
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = /* 0.15f */ 1.0f;

  double nextResetMinMax = 0.0;

  float minDelta = 1000.f;
  float maxDelta = 0.f;

  float minRenderDelta = 1000.f;

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickMouseLeftButton{glm::vec2(pos.x, pos.y)});
    }
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickMouseLeftButton{glm::vec2(pos.x, pos.y)});
    }
    if (IsKeyPressed(KEY_SPACE))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickSpace{glm::vec2(pos.x, pos.y)});
    }
    if (IsKeyPressed(KEY_LEFT_CONTROL))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickLeftControl{glm::vec2(pos.x, pos.y)});
    }

    if (IsKeyPressed(KEY_Q))
      ecs::send_event_broadcast(EventOnChangeCohesion{50.0f});
    if (IsKeyPressed(KEY_A))
      ecs::send_event_broadcast(EventOnChangeCohesion{-50.0f});
    if (IsKeyPressed(KEY_W))
      ecs::send_event_broadcast(EventOnChangeAlignment{50.0f});
    if (IsKeyPressed(KEY_S))
      ecs::send_event_broadcast(EventOnChangeAlignment{-50.0f});
    if (IsKeyPressed(KEY_E))
      ecs::send_event_broadcast(EventOnChangeSeparation{50.0f});
    if (IsKeyPressed(KEY_D))
      ecs::send_event_broadcast(EventOnChangeSeparation{-50.0f});
    if (IsKeyPressed(KEY_R))
      ecs::send_event_broadcast(EventOnChangeWander{50.0f});
    if (IsKeyPressed(KEY_F))
      ecs::send_event_broadcast(EventOnChangeWander{-50.0f});

    if (totalTime > nextResetMinMax)
    {
      nextResetMinMax = totalTime + 1.0;

      minDelta = 1000.f;
      maxDelta = 0.f;

      minRenderDelta = 1000.f;
    }

    if (dasCtx)
    {
      dasCtx->restart();
      dasCtx->restartHeaps(); 
    }

    reload_scripts();

    double t = GetTime();
    ecs::tick();
    // Clear FrameMem here because it might be used for Jobs
    // This is valid until all jobs live one frame
    clear_frame_mem();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    ecs::invoke_event_broadcast(EventUpdate{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    minDelta = eastl::min(minDelta, delta);
    maxDelta = eastl::max(maxDelta, delta);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode2D(camera);
    t = GetTime();
    ecs::invoke_event_broadcast(EventRender{});
    #ifdef _DEBUG
    ecs::invoke_event_broadcast(EventRenderDebug{});
    #endif
    const float renderDelta = (float)((GetTime() - t) * 1e3);
    minRenderDelta = eastl::min(minRenderDelta, renderDelta);
    EndMode2D();

    DrawText(FormatText("ECS time: %2.2f ms (%2.2f ms)", /* delta */minDelta, /* renderDelta */ minRenderDelta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawText(FormatText("FM: %d B Max: %d MB (%d kB)",
      get_frame_mem_allocated_size(),
      get_frame_mem_allocated_max_size() >> 20,
      get_frame_mem_allocated_max_size() >> 10),
      10, 70, 20, LIME);
    DrawFPS(10, 10);

    ecs::invoke_event_broadcast(EventRenderHUD{});

    // float w = screen_width;
    // float h = screen_height;
    // /* exitWindow = */ GuiWindowBox({ 0.f, 0.f, w*0.5f, h*0.5f }, "PORTABLE WINDOW");

    EndDrawing();
  }

  ecs::release();
  das::Module::Shutdown();

  extern void clear_textures();
  clear_textures();

  CloseWindow();

  stacktrace::release();

  return 0;
}