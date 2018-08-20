#include "scriptECS.h"

#include <assert.h>

#include <iostream>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scripthandle/scripthandle.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptmath/scriptmath.h>

REG_COMP_INIT(script::ScriptComponent, script);

namespace script
{
  ScriptSys::ScriptSys(asIScriptFunction *_fn) : RegSys(nullptr, -1, false), fn(_fn)
  {
    params = get_all_param_desc(fn);
  }

  void ScriptSys::init(const EntityManager *mgr)
  {
    const RegComp *comp = find_comp(params[0].type.c_str());
    assert(comp != nullptr);

    eventId = -1;
    stageId = comp ? comp->id : -1;

    for (size_t i = 0; i < params.size(); ++i)
    {
      const auto &param = params[i];

      componentNames.emplace_back() = param.name;
      componentTypeNames.emplace_back() = param.getTypeName();

      const RegComp *desc = find_comp(componentTypeNames.back().c_str());
      if (!desc)
        desc = find_comp(param.name.c_str());
      assert(desc != nullptr);
      components.push_back({ 0, mgr->getComponentNameId(param.name.c_str()), param.name.c_str(), desc });

      if ((param.flags & asTM_CONST) != 0)
        roComponents.push_back(i);
      else
        rwComponents.push_back(i);
    }
  }

  void ScriptSys::initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const
  {
    remap.resize(componentNames.size());
    eastl::fill(remap.begin(), remap.end(), -1);

    for (size_t compIdx = 0; compIdx < componentNames.size(); ++compIdx)
    {
      const char *name = componentNames[compIdx].c_str();
      const char *typeName = componentTypeNames[compIdx].c_str();
      const RegComp *desc = find_comp(typeName);
      if (!desc)
        desc = find_comp(name);

      for (size_t i = 0; i < template_comps.size(); ++i)
        if (template_comps[i].desc->id == desc->id && template_comps[i].name == name)
          remap[compIdx] = i;
    }
  }

  ScriptECS::ScriptECS()
  {
    eventCtx = internal::create_context();
    stageCtx = internal::create_context();

    callbackId = g_mgr->eventProcessCallbacks.size();
    g_mgr->eventProcessCallbacks.emplace_back() = [this](EntityId eid, int event_id, const RawArg &ev)
    {
      return sendEventSync(eid, event_id, ev);
    };
  }

  ScriptECS::~ScriptECS()
  {
    release();
  }

  bool ScriptECS::build(const char *path)
  {
    asIScriptModule *moudle = build_module(nullptr, path);
    asIScriptFunction *func = find_function_by_decl(moudle, "ref@ main()");

    eastl::vector<asIScriptFunction*> scriptSystems;
    call<CScriptHandle>([&scriptSystems](CScriptHandle *handle)
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

  void ScriptECS::release()
  {
    if (eventCtx)
      eventCtx->Release();
    eventCtx = nullptr;
    if (stageCtx)
      stageCtx->Release();
    stageCtx = nullptr;

    g_mgr->eventProcessCallbacks[callbackId] = nullptr;

    for (auto &sys : systems)
    {
      sys.fn->Release();
      sys.fn = nullptr;
    }

    queries.clear();
    systems.clear();
    remaps.clear();
  }

  void ScriptECS::invalidateQueries()
  {
    for (const auto &sys : systems)
    {
      auto &query = queries[sys.id];
      query.stageId = sys.stageId;
      query.sys = &sys;
      g_mgr->invalidateQuery(query);
    }
  }

  void ScriptECS::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
  {
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

          eventCtx->Prepare(sys.fn);
          internal::set_arg_wrapped(eventCtx, 0, event_id, ev.mem);
          for (int i = 1; i < (int)remap.size(); ++i)
          {
            Storage *storage = g_mgr->storages[templ.components[remap[i]].nameId];
            const int offset = entity.componentOffsets[remap[i]];
            internal::set_arg_wrapped(eventCtx, i, templ.components[remap[i]].desc, storage->getRaw(offset));
          }
          eventCtx->Execute();
        }
      }
  }

  void ScriptECS::tickStage(int stage_id, const RawArg &stage)
  {
    for (const auto &query : queries)
    {
      if (query.sys->stageId != stage_id)
        continue;
      for (const auto &eid : query.eids)
      {
        const auto &entity = g_mgr->entities[eid.index];
        const auto &templ = g_mgr->templates[entity.templateId];
        const auto &remap = remaps[entity.templateId][query.sys->id];

        stageCtx->Prepare(((ScriptSys*)query.sys)->fn);
        internal::set_arg_wrapped(stageCtx, 0, stage_id, stage.mem);
        for (int i = 1; i < (int)remap.size(); ++i)
        {
          Storage *storage = g_mgr->storages[templ.components[remap[i]].nameId];
          const int offset = entity.componentOffsets[remap[i]];
          internal::set_arg_wrapped(stageCtx, i, templ.components[remap[i]].desc, storage->getRaw(offset));
        }
        stageCtx->Execute();
      }
    }
  }
}
