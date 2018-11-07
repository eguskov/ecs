#pragma once

#include <ecs/ecs.h>

#include "script.h"

namespace script
{
  struct ScriptECS;

  struct ScriptSys
  {
    asIScriptFunction *fn = nullptr;

    bool useJoin = false;

    int id = -1;
    int stageId = -1;
    int eventId = -1;

    QueryDesc queryDesc;

    ParamDescVector params;

    eastl::vector<int> componentTypeIds;
    eastl::vector<eastl::string> componentNames;
    eastl::vector<eastl::string> componentTypeNames;

    ScriptSys(asIScriptFunction *_fn);

    void init(const EntityManager *mgr, const ScriptECS *script_ecs);
  };

  struct ScriptECS
  {
    bool loaded = false;

    asIScriptContext *eventCtx = nullptr;
    asIScriptContext *stageCtx = nullptr;

    eastl::vector<ScriptSys> systems;
    eastl::vector<JoinQuery> systemQueries;

    eastl::hash_map<TypeId, Query> dataQueries;

    ScriptECS() = default;
    ScriptECS(ScriptECS &&);
    ScriptECS(const ScriptECS&);
    ~ScriptECS();

    ScriptECS& operator=(ScriptECS&&);
    ScriptECS& operator=(const ScriptECS&);

    bool build(const char *name, const char *path);
    void release();

    void invalidateQueries();

    void sendEventSync(EntityId eid, int event_id, const RawArg &ev);
    void sendBroadcastEventSync(int event_id, const RawArg &ev);
    void tickStage(int stage_id, const RawArg &stage);

    template <typename S>
    void tick(const S &stage)
    {
      RawArgSpec<sizeof(S)> arg0;
      new (arg0.mem) S(stage);

      tickStage(RegCompSpec<S>::ID, arg0);
    }
  };

  struct ScriptComponent
  {
    eastl::string name;
    eastl::string path;
    ScriptECS scriptECS;

    ScriptComponent() = default;
    ScriptComponent(ScriptComponent &&) = default;
    ScriptComponent(const ScriptComponent&) = default;
    ~ScriptComponent() = default;

    ScriptComponent& operator=(ScriptComponent&&) = default;
    ScriptComponent& operator=(const ScriptComponent &assign) = default;

    bool set(const JFrameValue &value)
    {
      assert(value.HasMember("name"));
      name = value["name"].GetString();
      assert(value.HasMember("path"));
      path = value["path"].GetString();
      return true;
    }
  };
}

REG_COMP(script::ScriptComponent, script);