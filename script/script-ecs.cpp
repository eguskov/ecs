#include "script-ecs.h"

#include <ecs/debug.h>

#include <iostream>
#include <regex>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <scriptbuilder/scriptbuilder.h>

#include "script-query.h"

ECS_COMPONENT_TYPE_DETAILS_ALIAS(script::ScriptComponent, script);

namespace script
{
  ScriptSys::ScriptSys(asIScriptFunction *_fn) : fn(_fn)
  {
    params = get_all_param_desc(fn);
  }

  void ScriptSys::init(const EntityManager *mgr, const ScriptECS *script_ecs)
  {
    stageName = hash_str(params[0].type.c_str());

    for (size_t i = 1; i < params.size(); ++i)
    {
      const auto &param = params[i];

      componentNames.emplace_back() = param.name;
      componentTypeNames.emplace_back() = param.getTypeName();
      componentTypeIds.emplace_back() = param.typeId.id;

      const ComponentDescription *desc = find_component(componentTypeNames.back().c_str());
      if (!desc)
        desc = mgr->getComponentDescByName(param.name.c_str());
      ASSERT(desc != nullptr);
      queryDesc.components.push_back({ 0, hash_str(param.name.c_str()), desc->size, desc });
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
    dataQueries = assign.dataQueries;
    systemQueries = assign.systemQueries;

    for (auto &sys : systems)
      sys.fn->AddRef();

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
    dataQueries = eastl::move(assign.dataQueries);
    systemQueries = eastl::move(assign.systemQueries);

    return *this;
  }

  static void process_metadata(const JFrameDocument &doc, const char *key, eastl::vector<Component> &out_components)
  {
    if (!doc.HasMember(key))
      return;

    if (doc[key].IsArray())
    {
      for (int i = 0; i < (int)doc[key].Size(); ++i)
      {
        auto &c = out_components.emplace_back();
        c.desc = g_mgr->getComponentDescByName(doc[key][i].GetString());
        c.name = hash_str(doc[key][i].GetString());
        ASSERT(c.desc != nullptr);
      }
    }
    else
    {
      auto &c = out_components.emplace_back();
      c.desc = g_mgr->getComponentDescByName(doc[key].GetString());
      c.name = hash_str(doc[key].GetString());
      ASSERT(c.desc != nullptr);
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
    dataQueries.clear();
    systemQueries.clear();

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
          auto &query = dataQueries[typeId];

          query.name = hash_str(internal::get_engine()->GetTypeInfoById(typeId.id)->GetName());

          DEBUG_LOG("Add query: " << internal::get_engine()->GetTypeInfoById(typeId.id)->GetName());

          if (match[1].length() > 0)
          {
            JFrameDocument doc;
            doc.Parse(match[1].str().c_str());

            process_metadata(doc, "$have", query.desc.haveComponents);
            process_metadata(doc, "$not-have", query.desc.notHaveComponents);
          }

          ASSERT(type->GetPropertyCount() != 0 ||
            !query.desc.haveComponents.empty() ||
            !query.desc.notHaveComponents.empty());
          ASSERT(type->GetFactoryCount() != 0);

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

            const ComponentDescription *desc = find_component(typeName);
            if (!desc)
              desc = g_mgr->getComponentDescByName(name);
            ASSERT(desc != nullptr);

            query.desc.components.push_back({ i, hash_str(name), desc->size, desc });
          }

          g_mgr->performQuery(query);
        }
      }

      for (int i = 0; i < (int)module.GetFunctionCount(); ++i)
      {
        asIScriptFunction *fn = module.GetFunctionByIndex(i);
        eastl::string metadata = builder.GetMetadataStringForFunc(fn);

        DEBUG_LOG("Add system: " << fn->GetName());

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

    systemQueries.resize(systems.size());

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
  }

  void ScriptECS::invalidateQueries()
  {
    for (auto &it : dataQueries)
    {
      g_mgr->performQuery(it.second);
      DEBUG_LOG("invalidate query: " << internal::get_engine()->GetTypeInfoById(it.first.id)->GetName() << "; count: " << it.second.entitiesCount);
    }

    for (const auto &sys : systems)
    {
      auto &query = systemQueries[sys.id];
      query.desc = sys.queryDesc;

      g_mgr->performQuery(query);
      DEBUG_LOG("invalidate system query: " << sys.fn->GetName() << "; count: " << query.entitiesCount);
    }
  }

  void ScriptECS::sendEventSync(EntityId eid, uint32_t event_id, const RawArg &ev)
  {
    script::debug::attach(eventCtx);

    auto &entity = g_mgr->entities[eid.index];
    if (eid.generation != g_mgr->entityGenerations[eid.index])
      return;

    const auto &templ = g_mgr->templates[entity.templateId];
    const auto &archetype = g_mgr->archetypes[entity.archetypeId];

    for (const auto &sys : systems)
      if (sys.stageName.hash == event_id)
      {
        bool ok = true;
        for (const auto &c : sys.queryDesc.components)
          if (c.desc->id != g_mgr->eidCompId && c.desc->id != event_id && !archetype.hasCompontent(c.name))
          {
            ok = false;
            break;
          }

        if (ok)
        {
          eventCtx->Prepare(sys.fn);
          eventCtx->SetUserData(this, 1000);
          internal::set_arg_wrapped(eventCtx, 0, ev.mem);
          int compIdx = 0;
          for (const auto &c : sys.queryDesc.components)
          {
            auto &storage = archetype.storages[archetype.getComponentIndex(c.name)];
            internal::set_arg_wrapped(eventCtx, ++compIdx, storage->getRawByIndex(entity.indexInArchetype));
          }
          eventCtx->Execute();
        }
      }
  }

  void ScriptECS::sendBroadcastEventSync(uint32_t event_id, const RawArg &ev)
  {
    // TODO: Implementation
  }

  void ScriptECS::tickStage(uint32_t stage_id, const RawArg &stage)
  {
    script::debug::attach(stageCtx);

    // TODO: Store quries in map by stageId
    for (const auto &sys : systems)
    {
      if (sys.stageName.hash != stage_id)
        continue;

      auto &query = systemQueries[sys.id];
      for (int chunkIdx = 0; chunkIdx < query.chunksCount; ++chunkIdx)
      {
        for (int i = 0; i < query.entitiesInChunk[chunkIdx]; ++i)
        {
          stageCtx->Prepare(sys.fn);
          stageCtx->SetUserData(this, 1000);
          internal::set_arg_wrapped(stageCtx, 0, stage.mem);
          for (int compIdx = 0; compIdx < (int)query.desc.components.size(); ++compIdx)
          {
            QueryChunk &chunk = query.chunks[compIdx + chunkIdx * query.componentsCount];
            internal::set_arg_wrapped(stageCtx, compIdx + 1, chunk.beginData + i * query.desc.components[compIdx].size);
          }
          stageCtx->Execute();
        }
      }
    }
  }
}
