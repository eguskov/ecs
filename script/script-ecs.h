#pragma once

#include <ecs/ecs.h>

#include "script.h"

namespace script
{
  struct ScriptECS;

  struct ScriptSys
  {
    asIScriptFunction *fn = nullptr;

    int id = -1;
    HashedString stageName;

    QueryDescription queryDesc;

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
    eastl::vector<Query> systemQueries;

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

    void sendEventSync(EntityId eid, uint32_t event_id, const RawArg &ev);
    void sendBroadcastEventSync(uint32_t event_id, const RawArg &ev);
    void tickStage(uint32_t stage_id, const RawArg &stage);

    template <typename S>
    void tick(const S &stage)
    {
      RawArgSpec<sizeof(S)> arg0;
      new (arg0.mem) S(stage);

      tickStage(StageType<S>::id, arg0);
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
      ASSERT(value.HasMember("name"));
      name = value["name"].GetString();
      ASSERT(value.HasMember("path"));
      path = value["path"].GetString();
      return true;
    }
  };
};

ECS_COMPONENT_TYPE_ALIAS(script::ScriptComponent, script);