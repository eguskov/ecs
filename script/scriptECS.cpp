#include "scriptECS.h"

#include <assert.h>

#include <iostream>
#include <regex>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scripthandle/scripthandle.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptmath/scriptmath.h>
#include <scriptbuilder/scriptbuilder.h>

REG_COMP_INIT(script::ScriptComponent, script);

namespace script
{
  ScriptSys::ScriptSys(asIScriptFunction *_fn) : fn(_fn)
  {
    params = get_all_param_desc(fn);
  }

  void ScriptSys::init(const EntityManager *mgr, const ScriptECS *script_ecs)
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

      // auto res = script_ecs->queryDescs.find(param.typeId);
      // if (res != script_ecs->queryDescs.end())
      // {
      //   asITypeInfo *type = internal::get_engine()->GetTypeInfoById(param.typeId);
      // }

      const RegComp *desc = find_comp(componentTypeNames.back().c_str());
      if (!desc)
        desc = mgr->getComponentDescByName(param.name.c_str());
      assert(desc != nullptr);
      queryDesc.components.push_back({ 0, mgr->getComponentNameId(param.name.c_str()), desc });

      if ((param.flags & asTM_CONST) != 0)
        queryDesc.roComponents.push_back(i);
      else
        queryDesc.rwComponents.push_back(i);
    }
  }

  void ScriptSys::initRemap(const eastl::vector<CompDesc> &template_comps, RegSys::Remap &remap) const
  {
    remap.resize(componentNames.size());
    eastl::fill(remap.begin(), remap.end(), -1);

    for (size_t compIdx = 0; compIdx < componentNames.size(); ++compIdx)
    {
      const char *name = componentNames[compIdx].c_str();
      const char *typeName = componentTypeNames[compIdx].c_str();
      const RegComp *desc = find_comp(typeName);
      if (!desc)
        desc = g_mgr->getComponentDescByName(name);
      assert(desc != nullptr);

      for (size_t i = 0; i < template_comps.size(); ++i)
        if (template_comps[i].desc->id == desc->id && template_comps[i].nameId == g_mgr->getComponentNameId(name))
          remap[compIdx] = i;
    }
  }

  ScriptECS::~ScriptECS()
  {
    release();
  }

  ScriptECS::ScriptECS(const ScriptECS &assign)
  {
    (*this) = assign;
  }

  ScriptECS& ScriptECS::operator=(const ScriptECS &assign)
  {
    if (this == &assign)
      return *this;

    release();

    eventCtx = assign.eventCtx;
    stageCtx = assign.stageCtx;

    eventCtx->AddRef();
    stageCtx->AddRef();

    loaded = assign.loaded;

    systems = assign.systems;
    remaps = assign.remaps;
    dataQueries = assign.dataQueries;
    systemQueries = assign.systemQueries;

    for (auto &sys : systems)
      sys.fn->AddRef();

    // queryDescs = assign.queryDescs;

    return *this;
  }

  ScriptECS::ScriptECS(ScriptECS &&assign)
  {
    (*this) = eastl::move(assign);
  }

  ScriptECS& ScriptECS::operator=(ScriptECS &&assign)
  {
    release();

    eventCtx = assign.eventCtx;
    stageCtx = assign.stageCtx;

    assign.eventCtx = nullptr;
    assign.stageCtx = nullptr;

    loaded = assign.loaded;

    systems = eastl::move(assign.systems);
    remaps = eastl::move(assign.remaps);
    dataQueries = eastl::move(assign.dataQueries);
    systemQueries = eastl::move(assign.systemQueries);

    // queryDescs = eastl::move(assign.queryDescs);

    return *this;
  }

  static void process_metadata(const JFrameDocument &doc, const char *key, eastl::vector<CompDesc> &out_components)
  {
    if (!doc.HasMember(key))
      return;

    if (doc[key].IsArray())
    {
      for (int i = 0; i < (int)doc[key].Size(); ++i)
      {
        auto &c = out_components.emplace_back();
        c.desc = g_mgr->getComponentDescByName(doc[key][i].GetString());
        c.nameId = g_mgr->getComponentNameId(doc[key][i].GetString());
        assert(c.desc != nullptr);
      }
    }
    else
    {
      auto &c = out_components.emplace_back();
      c.desc = g_mgr->getComponentDescByName(doc[key].GetString());
      c.nameId = g_mgr->getComponentNameId(doc[key].GetString());
      assert(c.desc != nullptr);
    }
  }

  bool ScriptECS::build(const char *name, const char *path)
  {
    for (auto &sys : systems)
    {
      sys.fn->Release();
      sys.fn = nullptr;
    }

    systems.clear();
    remaps.clear();
    dataQueries.clear();
    systemQueries.clear();
    // queryDescs.clear();

    bool isOk = build_module(name, path, [&](CScriptBuilder &builder, asIScriptModule &module)
    {
      for (int i = 0; i < (int)module.GetObjectTypeCount(); ++i)
      {
        asITypeInfo *type = module.GetObjectTypeByIndex(i);
        TypeId typeId = type->GetTypeId();
        eastl::string metadata = builder.GetMetadataStringForType(typeId.id);

        std::cmatch match;
        std::regex re = std::regex("(?:\\s*)\\bquery\\b(?:\\s*)(.*)");
        std::regex_search(metadata.cbegin(), metadata.cend(), match, re);

        if (match.size() > 0)
        {
          Query &query = dataQueries[typeId];

          // query.desc.type = type;

          if (match[1].length() > 0)
          {
            JFrameDocument doc;
            doc.Parse(match[1].str().c_str());

            process_metadata(doc, "$is-true", query.desc.isTrueComponents);
            process_metadata(doc, "$is-false", query.desc.isFalseComponents);
            process_metadata(doc, "$have", query.desc.haveComponents);
            process_metadata(doc, "$not-have", query.desc.notHaveComponents);
          }

          assert(type->GetPropertyCount() != 0 ||
            !query.desc.isTrueComponents.empty() ||
            !query.desc.isFalseComponents.empty() ||
            !query.desc.haveComponents.empty() ||
            !query.desc.notHaveComponents.empty());
          assert(type->GetFactoryCount() != 0);

          for (int i = 0; i < (int)type->GetPropertyCount(); ++i)
          {
            const char *name = nullptr;
            int typeId = -1;
            type->GetProperty(i, &name, &typeId);

            const char *typeName = nullptr;

            if (typeId == asTYPEID_FLOAT) typeName = "float";
            else if (typeId == asTYPEID_INT32) typeName = "int";
            else if (typeId == asTYPEID_BOOL) typeName = "bool";
            else typeName = internal::get_engine()->GetTypeInfoById(typeId)->GetName();

            const RegComp *desc = find_comp(typeName);
            if (!desc)
              desc = find_comp(name);
            assert(desc != nullptr);

            query.desc.components.push_back({ i, g_mgr->getComponentNameId(name), desc });
          }
        }
      }

      for (int i = 0; i < (int)module.GetFunctionCount(); ++i)
      {
        asIScriptFunction *fn = module.GetFunctionByIndex(i);
        eastl::string metadata = builder.GetMetadataStringForFunc(fn);

        std::cmatch match;
        std::regex re = std::regex("(?:\\s*)\\b(system|on_load|on_reload)\\b(?:\\s*)(.*)");
        std::regex_search(metadata.cbegin(), metadata.cend(), match, re);

        if (match.size() > 0)
        {
          if (match[1].str() == "system")
          {
            auto &sys = systems.emplace_back(fn);
            fn->AddRef();

            if (match[2].length() > 0)
            {
              JFrameDocument doc;
              doc.Parse(match[2].str().c_str());
              process_metadata(doc, "$is-true", sys.queryDesc.isTrueComponents);
              process_metadata(doc, "$is-false", sys.queryDesc.isFalseComponents);
              process_metadata(doc, "$have", sys.queryDesc.haveComponents);
              process_metadata(doc, "$not-have", sys.queryDesc.notHaveComponents);
            }
          }
          else if (match[1].str() == "on_load")
          {
            if (!loaded)
              call(this, fn);
          }
          else if (match[1].str() == "on_reload")
          {
            if (loaded)
              call(this, fn);
          }
        }
      }

      if (asIScriptFunction *mainFn = find_function_by_decl(&module, "void main()"))
        call(mainFn);
    });

    if (!isOk)
      return false;

    if (!loaded)
    {
      eventCtx = internal::create_context();
      stageCtx = internal::create_context();
    }

    int id = 0;
    for (auto &sys : systems)
    {
      sys.id = id++;
      sys.init(g_mgr, this);
    }

    remaps.resize(g_mgr->templates.size());
    for (auto &remap : remaps)
      remap.resize(systems.size());

    systemQueries.resize(systems.size());

    for (int templateId = 0; templateId < (int)g_mgr->templates.size(); ++templateId)
      for (int sysId = 0; sysId < (int)systems.size(); ++sysId)
        systems[sysId].initRemap(g_mgr->templates[templateId].components, remaps[templateId][sysId]);

    invalidateQueries();

    loaded = true;

    return true;
  }

  void ScriptECS::release()
  {
    if (eventCtx)
      eventCtx->Release();
    if (stageCtx)
      stageCtx->Release();

    eventCtx = nullptr;
    stageCtx = nullptr;

    for (auto &sys : systems)
    {
      sys.fn->Release();
      sys.fn = nullptr;
    }

    dataQueries.clear();
    systemQueries.clear();
    systems.clear();
    remaps.clear();
    // queryDescs.clear();
  }

  void ScriptECS::invalidateQueries()
  {
    for (auto &it : dataQueries)
      g_mgr->invalidateQuery(it.second);

    for (const auto &sys : systems)
    {
      auto &query = systemQueries[sys.id];
      query.stageId = sys.stageId;
      query.sysId = sys.id;
      query.desc = sys.queryDesc;
      g_mgr->invalidateQuery(query);
    }
  }

  void ScriptECS::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
  {
    auto &entity = g_mgr->entities[eid2idx(eid)];
    if (eid.generation != entity.eid.generation)
      return;
    const auto &templ = g_mgr->templates[entity.templateId];

    for (const auto &sys : systems)
      if (sys.stageId == event_id)
      {
        bool ok = true;
        for (const auto &c : sys.queryDesc.components)
          if (c.desc->id != g_mgr->eidCompId && c.desc->id != event_id && !templ.hasCompontent(c.desc->id, c.nameId))
          {
            ok = false;
            break;
          }

        if (ok)
        {
          const auto &remap = remaps[entity.templateId][sys.id];

          eventCtx->Prepare(sys.fn);
          eventCtx->SetUserData(this, 1000);
          internal::set_arg_wrapped(eventCtx, 0, ev.mem);
          for (int i = 1; i < (int)remap.size(); ++i)
          {
            Storage *storage = g_mgr->storages[templ.components[remap[i]].nameId];
            const int offset = entity.componentOffsets[remap[i]];
            internal::set_arg_wrapped(eventCtx, i, storage->getRaw(offset));
          }
          eventCtx->Execute();
        }
      }
  }

  void ScriptECS::tickStage(int stage_id, const RawArg &stage)
  {
    // TODO: Store quries in map by stageId
    for (const auto &query : systemQueries)
    {
      if (query.stageId != stage_id)
        continue;
      for (const auto &eid : query.eids)
      {
        const auto &entity = g_mgr->entities[eid.index];
        const auto &templ = g_mgr->templates[entity.templateId];
        const auto &remap = remaps[entity.templateId][query.sysId];

        stageCtx->Prepare(systems[query.sysId].fn);
        stageCtx->SetUserData(this, 1000);
        internal::set_arg_wrapped(stageCtx, 0, stage.mem);
        for (int i = 1; i < (int)remap.size(); ++i)
        {
          Storage *storage = g_mgr->storages[templ.components[remap[i]].nameId];
          const int offset = entity.componentOffsets[remap[i]];
          internal::set_arg_wrapped(stageCtx, i, storage->getRaw(offset));
        }
        stageCtx->Execute();
      }
    }
  }
}
