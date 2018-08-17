#include <random>
#include <ctime>

#include <ecs/ecs.h>
#include <ecs/perf.h>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "update.ecs.h"

#include <script.h>

#include <scripthandle/scripthandle.h>
#include <scriptarray/scriptarray.h>

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

struct ScriptSys : RegSys
{
  asIScriptFunction *fn = nullptr;

  script::ParamDescVector params;

  eastl::vector<eastl::string> componentNames;
  eastl::vector<eastl::string> componentTypeNames;

  ScriptSys(asIScriptFunction *_fn) : RegSys(nullptr, -1, false), fn(_fn)
  {
    params = script::get_all_param_desc(fn);
  }

  void init(const EntityManager *mgr) override final
  {
    const RegComp *comp = find_comp(params[0].type.c_str());
    assert(comp != nullptr);

    eventId = -1;
    stageId = comp ? comp->id : -1;

    for (size_t i = 0; i < params.size(); ++i)
    {
      const auto &param = params[i];

      componentNames.emplace_back() = param.name;
      componentTypeNames.emplace_back() = param.type;

      const RegComp *desc = find_comp(param.type.c_str());
      assert(desc != nullptr);
      components.push_back({ 0, g_mgr->getComponentNameId(param.name.c_str()), param.name.c_str(), desc });

      if ((param.flags & asTM_CONST) != 0)
        roComponents.push_back(i);
      else
        rwComponents.push_back(i);
    }
  }

  void initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const override final
  {
    remap.resize(componentNames.size());
    eastl::fill(remap.begin(), remap.end(), -1);

    for (size_t compIdx = 0; compIdx < componentNames.size(); ++compIdx)
    {
      const char *name = componentNames[compIdx].c_str();
      const char *typeName = componentTypeNames[compIdx].c_str();
      const RegComp *desc = find_comp(typeName);

      for (size_t i = 0; i < template_comps.size(); ++i)
        if (template_comps[i].desc->id == desc->id && template_comps[i].name == name)
          remap[compIdx] = i;
    }
  }
};

static glm::vec2 opAssign(const glm::vec2& v1)
{
}

static glm::vec2 opAdd(const glm::vec2& v1, const glm::vec2& v2)
{
  return v1 + v2;
}

struct ScriptECS
{
  eastl::vector<ScriptSys> systems;
  eastl::vector<eastl::vector<RegSys::Remap>> remaps;
  eastl::vector<Query> queries;

  bool init(const char *path)
  {
    g_mgr->eventProcessCallback = [this](EntityId eid, int event_id, const RawArg &ev)
    {
      return sendEventSync(eid, event_id, ev);
    };

    script::init();

    script::register_struct<UpdateStage, 1>("UpdateStage");
    script::register_struct_property("UpdateStage", "float dt", offsetof(UpdateStage, dt));
    script::register_struct_property("UpdateStage", "float total", offsetof(UpdateStage, total));

    script::register_struct<EventOnKillEnemy, 1>("EventOnKillEnemy");

    glm::vec2 a, b;
    a + b;

    script::register_struct<glm::vec2, 256>("vec2");
    script::register_struct_property("vec2", "float x", offsetof(glm::vec2, x));
    script::register_struct_property("vec2", "float y", offsetof(glm::vec2, y));
    script::register_struct_method("vec2", "vec2& opAssign(const vec2&in)", asMETHODPR(glm::vec2, operator=, (const glm::vec2&), glm::vec2&));
    script::register_function("vec2", "vec2 opAdd(const vec2&in) const", asFUNCTIONPR(opAdd, (const glm::vec2&, const glm::vec2&), glm::vec2));

    asIScriptModule *moudle = script::build_module(nullptr, path);
    asIScriptFunction *func = script::find_function_by_decl(moudle, "ref@ main()");

    eastl::vector<asIScriptFunction*> scriptSystems;
    script::call<CScriptHandle>([&scriptSystems](CScriptHandle *handle)
    {
      CScriptArray *systems = (CScriptArray*)handle->GetRef();
      for (int i = 0; i < (int)systems->GetSize(); ++i)
      {
        asIScriptFunction* fn = (asIScriptFunction*)((CScriptHandle*)systems->At(i))->GetRef();
        fn->AddRef();
        fn->AddRef();
        scriptSystems.push_back(fn);
      }
    }, func);

    for (auto fn : scriptSystems)
      systems.emplace_back(fn);

    int id = 0;
    for (auto &sys : systems)
    {
      sys.id = id++;
      sys.init(g_mgr);
    }

    remaps.resize(g_mgr->templates.size());
    for (auto &remap : remaps)
      remap.resize(systems.size());

    queries.resize(systems.size());

    for (int templateId = 0; templateId < (int)g_mgr->templates.size(); ++templateId)
      for (int sysId = 0; sysId < (int)systems.size(); ++sysId)
        systems[sysId].initRemap(g_mgr->templates[templateId].components, remaps[templateId][sysId]);

    return true;
  }

  void release()
  {
    for (auto &sys : systems)
    {
      sys.fn->Release();
      sys.fn = nullptr;
    }

    queries.clear();
    systems.clear();
    remaps.clear();

    script::release();
  }

  void invalidateQueries()
  {
    for (const auto &sys : systems)
    {
      auto &query = queries[sys.id];
      query.stageId = sys.stageId;
      query.sys = &sys;
      g_mgr->invalidateQuery(query);
    }
  }

  void sendEventSync(EntityId eid, int event_id, const RawArg &ev)
  {
    // TODO: Create context once
    script::ScopeContext ctx;

    auto &entity = g_mgr->entities[eid2idx(eid)];
    const auto &templ = g_mgr->templates[entity.templateId];

    for (const auto &sys : systems)
      if (sys.stageId == event_id)
      {
        bool ok = true;
        for (const auto &c : sys.components)
          if (c.desc->id != g_mgr->eidCompId && c.desc->id != event_id && !templ.hasCompontent(c.desc->id, c.name.c_str()))
          {
            ok = false;
            break;
          }

        if (ok)
        {
          const auto &remap = remaps[entity.templateId][sys.id];

          ctx->Prepare(sys.fn);
          ctx->SetArgAddress(0, (void*)ev.mem);
          for (int i = 1; i < (int)remap.size(); ++i)
            ctx->SetArgAddress(i, g_mgr->storages[templ.components[remap[i]].nameId]->getRaw(entity.componentOffsets[remap[i]]));
          ctx->Execute();
        }
      }
  }

  template <typename S>
  void tick(const S &stage)
  {
    // TODO: Create context once
    script::ScopeContext ctx;
    for (const auto &query : queries)
    {
      if (query.sys->stageId != RegCompSpec<S>::ID)
        continue;
      for (const auto &eid : query.eids)
      {
        const auto &entity = g_mgr->entities[eid.index];
        const auto &templ = g_mgr->templates[entity.templateId];
        const auto &remap = remaps[entity.templateId][query.sys->id];

        ctx->Prepare(((ScriptSys*)query.sys)->fn);
        ctx->SetArgAddress(0, (void*)&stage);
        for (int i = 1; i < (int)remap.size(); ++i)
          ctx->SetArgAddress(i, g_mgr->storages[templ.components[remap[i]].nameId]->getRaw(entity.componentOffsets[remap[i]]));
        ctx->Execute();
      }
    }
  }
};

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

  ScriptECS scriptECS;
  scriptECS.init("script.as");

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

    if (g_mgr->status != kStatusNone)
      scriptECS.invalidateQueries();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    g_mgr->tick(UpdateStage{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    scriptECS.tick(UpdateStage{ dt, totalTime });

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

  scriptECS.release();
  EntityManager::release();

  CloseWindow();

  return 0;
}