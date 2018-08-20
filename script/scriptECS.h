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

  struct ScriptECS
  {
    int callbackId = -1;

    asIScriptContext *eventCtx = nullptr;
    asIScriptContext *stageCtx = nullptr;

    eastl::vector<ScriptSys> systems;
    eastl::vector<eastl::vector<RegSys::Remap>> remaps;
    eastl::vector<Query> queries;

    ScriptECS();
    ~ScriptECS();

    bool build(const char *path);
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
    eastl::string path;
    ScriptECS scriptECS;

    bool set(const JValue &value)
    {
      assert(value.HasMember("path"));
      path = value["path"].GetString();
      return true;
    }
  };
}

REG_COMP(script::ScriptComponent, script);