#include "stdafx.h"

#include <random>
#include <ctime>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "ecs/ecs.h"

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "systems/update.ecs.h"

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

int main()
{
  {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    int status = luaL_loadfile(L, "script.lua");
    if (status)
    {
      fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
      // exit(1);
    }

    lua_newtable(L);

    for (int i = 1; i <= 5; i++)
    {
      lua_pushnumber(L, i);
      lua_pushnumber(L, i * 2);
      lua_rawset(L, -3);
    }

    lua_setglobal(L, "foo");

    int result = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (result)
    {
      fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
      // exit(1);
    }

    double sum = lua_tonumber(L, -1);

    printf("Script returned: %.0f\n", sum);

    lua_pop(L, 1);
    lua_close(L);
  }

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

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    /*if (IsKeyReleased(KEY_SPACE))
    {
      eastl::vector<EntityId> eids;
      g_mgr->queryEids(eids, { { "timer", "timer" } });
      g_mgr->sendEvent(eids[0], EventOnSpawn{1000});
    }*/
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

  CloseWindow();

  return 0;
}