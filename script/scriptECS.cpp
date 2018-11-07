#include "scriptECS.h"

#include <ecs/debug.h>

#include <iostream>
#include <regex>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <scriptbuilder/scriptbuilder.h>

#include "scriptQuery.h"

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
    ASSERT(comp != nullptr);

    eventId = -1;
    stageId = comp ? comp->id : -1;

    for (size_t i = 1; i < params.size(); ++i)
    {
      const auto &param = params[i];

      componentNames.emplace_back() = param.name;
      componentTypeNames.emplace_back() = param.getTypeName();
      componentTypeIds.emplace_back() = param.typeId.id;

      auto res = script_ecs->dataQueries.find(param.typeId);
      if (res != script_ecs->dataQueries.end())
      {
        useJoin = true;
        queryDesc.joinQueries.push_back(param.typeId.id);
      }
      else
      {
        ASSERT(!useJoin);

        const RegComp *desc = find_comp(componentTypeNames.back().c_str());
        if (!desc)
          desc = mgr->getComponentDescByName(param.name.c_str());
        ASSERT(desc != nullptr);
        queryDesc.components.push_back({ 0, hash_str(param.name.c_str()), desc->size, desc });

        if ((param.flags & asTM_CONST) != 0)
          queryDesc.roComponents.push_back(i);
        else
          queryDesc.rwComponents.push_back(i);
      }
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

  static void process_join(const JFrameDocument &doc, ScriptECS *script_ecs, const asIScriptModule &module, QueryDesc &query_desc)
  {
    if (!doc.HasMember("$join"))
      return;

    const JFrameValue &join = doc["$join"];

    ASSERT(join.IsArray() && join.Size() == 2);

    eastl::string first = join[0].GetString();
    eastl::string second = join[1].GetString();

    std::cmatch match;
    std::regex re = std::regex("(.*)\\.(.*)");

    QueryLink &link = query_desc.joinLinks.emplace_back();

    {
      std::regex_search(first.cbegin(), first.cend(), match, re);
      ASSERT(match.size() == 3);

      asITypeInfo *type = module.GetTypeInfoByName(match[1].str().c_str());
      ASSERT(type != nullptr);

      link.firstTypeId = type->GetTypeId();
      link.firstComponentName = hash_str(match[2].str().c_str());

      auto res = script_ecs->dataQueries.find(link.firstTypeId);
      ASSERT(res != script_ecs->dataQueries.end());

      ASSERT(res->second.desc.getComponentIndex(link.firstComponentName) >= 0);
    }

    {
      std::regex_search(second.cbegin(), second.cend(), match, re);
      ASSERT(match.size() == 3);

      asITypeInfo *type = module.GetTypeInfoByName(match[1].str().c_str());
      ASSERT(type != nullptr);

      link.secondTypeId = type->GetTypeId();
      link.secondComponentName = hash_str(match[2].str().c_str());

      auto res = script_ecs->dataQueries.find(link.secondTypeId);
      ASSERT(res != script_ecs->dataQueries.end());

      ASSERT(res->second.desc.getComponentIndex(link.secondComponentName) >= 0);
    }

    ASSERT(link.firstTypeId >= 0 && link.firstComponentName.hash >= 0);
    ASSERT(link.secondTypeId >= 0 && link.secondComponentName.hash >= 0);
    ASSERT(g_mgr->getComponentDescByName(link.firstComponentName) != nullptr);
    ASSERT(g_mgr->getComponentDescByName(link.secondComponentName) != nullptr);
    ASSERT(g_mgr->getComponentDescByName(link.firstComponentName)->size == g_mgr->getComponentDescByName(link.secondComponentName)->size);
    link.componentDesc = g_mgr->getComponentDescByName(link.firstComponentName);
    link.componentSize = link.componentDesc->size;
    ASSERT(link.componentDesc->hasEqual);
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

            process_metadata(doc, "$is-true", query.desc.isTrueComponents);
            process_metadata(doc, "$is-false", query.desc.isFalseComponents);
            process_metadata(doc, "$have", query.desc.haveComponents);
            process_metadata(doc, "$not-have", query.desc.notHaveComponents);

            for (const auto &c : query.desc.isTrueComponents)
              g_mgr->enableChangeDetection(c.name);
            for (const auto &c : query.desc.isFalseComponents)
              g_mgr->enableChangeDetection(c.name);
          }

          ASSERT(type->GetPropertyCount() != 0 ||
            !query.desc.isTrueComponents.empty() ||
            !query.desc.isFalseComponents.empty() ||
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

            const RegComp *desc = find_comp(typeName);
            if (!desc)
              desc = g_mgr->getComponentDescByName(name);
            ASSERT(desc != nullptr);

            query.desc.components.push_back({ i, hash_str(name), desc->size, desc });
            query.componentsCount = query.desc.components.size();
          }

          g_mgr->invalidateQuery(query);
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
              process_metadata(doc, "$is-true", sys.queryDesc.isTrueComponents);
              process_metadata(doc, "$is-false", sys.queryDesc.isFalseComponents);
              process_metadata(doc, "$have", sys.queryDesc.haveComponents);
              process_metadata(doc, "$not-have", sys.queryDesc.notHaveComponents);
              process_join(doc, this, module, sys.queryDesc);

              for (const auto &c : sys.queryDesc.isTrueComponents)
                g_mgr->enableChangeDetection(c.name);
              for (const auto &c : sys.queryDesc.isFalseComponents)
                g_mgr->enableChangeDetection(c.name);
              for (const auto &link : sys.queryDesc.joinLinks)
              {
                g_mgr->enableChangeDetection(link.firstComponentName);
                g_mgr->enableChangeDetection(link.secondComponentName);
              }
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
      g_mgr->invalidateQuery(it.second);
      DEBUG_LOG("invalidate query: " << internal::get_engine()->GetTypeInfoById(it.first.id)->GetName() << "; count: " << it.second.entitiesCount);
    }

    for (const auto &sys : systems)
    {
      auto &query = systemQueries[sys.id];
      query.stageId = sys.stageId;
      query.sysId = sys.id;
      query.desc = sys.queryDesc;
      if (sys.useJoin)
      {
        query.eids.clear();

        for (const QueryLink &link : query.desc.joinLinks)
        {
          auto first = dataQueries.find(link.firstTypeId);
          auto second = dataQueries.find(link.secondTypeId);
          ASSERT(first != dataQueries.end());
          ASSERT(second != dataQueries.end());

          auto &firstQuery = first->second;
          auto &secondQuery = second->second;
          auto firstEid = firstQuery.iter<EntityId>(hash::cstr("eid")).begin();
          for (const auto &firstRaw : firstQuery.iter(link.firstComponentName))
          {
            auto secondEid = secondQuery.iter<EntityId>(hash::cstr("eid")).begin();
            for (const auto &secondRaw : secondQuery.iter(link.secondComponentName))
            {
              // if (::memcmp(firstRaw, secondRaw, link.componentSize) == 0)
              if (link.componentDesc->equal(firstRaw, secondRaw))
              {
                query.eids.emplace_back() = *firstEid;
                query.eids.emplace_back() = *secondEid;
              }
              ++secondEid;
            }
            ++firstEid;
          }
        }

        ASSERT((query.eids.size() % query.desc.joinQueries.size()) == 0);
        DEBUG_LOG("invalidate system join query: " << sys.fn->GetName() << "; count: " << query.entitiesCount);
      }
      else
      {
        g_mgr->invalidateQuery(query);
        DEBUG_LOG("invalidate system query: " << sys.fn->GetName() << "; count: " << query.entitiesCount);
      }
    }
  }

  void ScriptECS::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
  {
    auto &entity = g_mgr->entities[eid2idx(eid)];
    if (eid.generation != entity.eid.generation)
      return;

    const auto &templ = g_mgr->templates[entity.templateId];
    const auto &archetype = g_mgr->archetypes[entity.archetypeId];

    for (const auto &sys : systems)
      if (sys.stageId == event_id)
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
            internal::set_arg_wrapped(eventCtx, ++compIdx, storage.storage->getRawByIndex(entity.indexInArchetype));
          }
          eventCtx->Execute();
        }
      }
  }

  void ScriptECS::sendBroadcastEventSync(int event_id, const RawArg &ev)
  {
    // TODO: Implementation
  }

  void ScriptECS::tickStage(int stage_id, const RawArg &stage)
  {
    // TODO: Store quries in map by stageId
    for (auto &query : systemQueries)
    {
      if (query.stageId != stage_id)
        continue;
      // TODO: Remove branch somehow if it performance critical
      // TODO: Check join queries intersection for debug
      if (systems[query.sysId].useJoin)
      {
        int firstQueryIndex = 0;
        uint32_t firstTypeId = query.desc.joinQueries[firstQueryIndex];

        asITypeInfo *firstType = internal::get_engine()->GetTypeInfoById(firstTypeId);
        ASSERT(firstType != nullptr);

        auto firstDataQuery = dataQueries.find(TypeId{ firstTypeId });
        ASSERT(firstDataQuery != dataQueries.end());

        int secondQueryIndex = 1;
        uint32_t secondTypeId = query.desc.joinQueries[secondQueryIndex];

        asITypeInfo *secondType = internal::get_engine()->GetTypeInfoById(secondTypeId);
        ASSERT(secondType != nullptr);

        auto secondDataQuery = dataQueries.find(TypeId{ secondTypeId });
        ASSERT(secondDataQuery != dataQueries.end());

        if (query.desc.joinLinks.empty())
        {
          for (const auto &firstEid : firstDataQuery->second.iter<EntityId>(hash::cstr("eid")))
          {
            asIScriptObject *firstObject = inject_components_into_struct(firstEid, firstDataQuery->second.desc.components, firstType);
            ASSERT(firstObject != nullptr);

            for (const auto &secondEid : secondDataQuery->second.iter<EntityId>(hash::cstr("eid")))
            {
              asIScriptObject *secondObject = inject_components_into_struct(secondEid, secondDataQuery->second.desc.components, secondType);
              ASSERT(secondObject != nullptr);

              stageCtx->Prepare(systems[query.sysId].fn);
              stageCtx->SetUserData(this, 1000);

              internal::set_arg_wrapped(stageCtx, 0, stage.mem);
              firstObject->AddRef();
              internal::set_arg_wrapped(stageCtx, 1 + firstQueryIndex, firstObject);
              internal::set_arg_wrapped(stageCtx, 1 + secondQueryIndex, secondObject);

              stageCtx->Execute();
            }

            firstObject->Release();
          }
        }
        else
        {
          const int step = query.desc.joinQueries.size();
          ASSERT(step == 2);
          for (int i = 0; i < (int)query.eids.size(); i += step)
          {
            const EntityId &firstEid = query.eids[i + 0];
            const EntityId &secondEid = query.eids[i + 1];

            asIScriptObject *firstObject = inject_components_into_struct(firstEid, firstDataQuery->second.desc.components, firstType);
            ASSERT(firstObject != nullptr);

            asIScriptObject *secondObject = inject_components_into_struct(secondEid, secondDataQuery->second.desc.components, secondType);
            ASSERT(secondObject != nullptr);

            stageCtx->Prepare(systems[query.sysId].fn);
            stageCtx->SetUserData(this, 1000);

            internal::set_arg_wrapped(stageCtx, 0, stage.mem);
            internal::set_arg_wrapped(stageCtx, 1 + firstQueryIndex, firstObject);
            internal::set_arg_wrapped(stageCtx, 1 + secondQueryIndex, secondObject);

            stageCtx->Execute();
          }
        }
      }
      else
      {
        for (int chunkIdx = 0; chunkIdx < query.chunksCount; ++chunkIdx)
        {
          for (int i = 0; i < query.entitiesInChunk[chunkIdx]; ++i)
          {
            stageCtx->Prepare(systems[query.sysId].fn);
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
}
