#pragma once

#include <ecs/ecs.h>

#include "script.h"

namespace script
{
  struct ScriptSys : RegSys
  {
    asIScriptFunction *fn = nullptr;

    ParamDescVector params;

    eastl::vector<eastl::string> componentNames;
    eastl::vector<eastl::string> componentTypeNames;

    ScriptSys(asIScriptFunction *_fn);

    void init(const EntityManager *mgr) override final;
    void initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const override final;
  };

  struct ScriptQueryDesc
  {
    asITypeInfo *type = nullptr;
    eastl::vector<CompDesc> components;
    eastl::vector<CompDesc> haveComponents;
    eastl::vector<CompDesc> notHaveComponents;
    eastl::vector<CompDesc> isTrueComponents;
    eastl::vector<CompDesc> isFalseComponents;
  };

  struct ScriptECS
  {
    bool loaded = false;

    asIScriptContext *eventCtx = nullptr;
    asIScriptContext *stageCtx = nullptr;

    eastl::vector<ScriptSys> systems;
    eastl::vector<eastl::vector<RegSys::Remap>> remaps;
    eastl::vector<Query> queries;

    eastl::hash_map<int, ScriptQueryDesc> queryDescs;

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