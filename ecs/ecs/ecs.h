#pragma once

#include "stdafx.h"

#include <future>

#include "io/json.h"

#include "entity.h"
#include "component.h"
#include "system.h"

#include "event.h"
#include "stage.h"

#include "components/eid.component.h"

struct RegSys;
extern RegSys *reg_sys_head;
extern int reg_sys_count;

struct RegComp;
extern RegComp *reg_comp_head;
extern int reg_comp_count;

struct EventOnEntityCreate : Event
{
};
REG_EVENT(EventOnEntityCreate);

struct EventOnEntityReady : Event
{
};
REG_EVENT(EventOnEntityReady);

struct Template
{
  int size = 0;
  int storageId = -1;
  int docId = -1;

  eastl::string name;
  eastl::bitvector<> compMask;
  eastl::vector<CompDesc> components;
  eastl::vector<RegSys::Remap> remaps;

  bool hasCompontent(int id, const char *name) const;
  int getCompontentOffset(int id, const char *name) const;
};

template <typename T>
struct Cleanup
{
  using Type = typename eastl::remove_const<typename eastl::remove_reference<T>::type>::type;
};

struct Entity
{
  bool ready = false;
  EntityId eid;
  int templateId = -1;
  int memId = -1;
};

struct EntitySoA
{
  bool ready = false;

  EntityId eid;

  int templateId = -1;

  eastl::fixed_vector<int, 256, false> componentOffsets;
};

struct System
{
  int id;
  const RegSys *desc;
};

struct EventStream
{
  int popOffset = 0;
  int pushOffset = 0;
  int count = 0;
  eastl::vector<uint8_t> data;

  void push(EntityId eid, int event_id, const RawArg &ev);
  eastl::tuple<EntityId, int, RawArg> pop();
};

struct CreateQueueData
{
  eastl::string templanemName;
  JDocument components;
};

struct AsyncValue
{
  EntityId eid;
  std::future<bool> value;

  AsyncValue(EntityId _eid, std::future<bool> &&_value) : eid(_eid), value(eastl::move(_value)) {}

  bool isReady() const
  {
    return value.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
  }
};

struct Query
{
  int stageId = -1;
  const RegSys *sys = nullptr;
  EntityVector eids;
};

struct EntityManager
{
  int eidCompId = -1;

  JDocument templatesDoc;

  eastl::vector<eastl::string> order;
  eastl::vector<Template> templates;
  eastl::vector<Storage> storages;
  eastl::vector<Storage> storagesSoA;
  eastl::vector<Entity> entities;
  eastl::vector<EntitySoA> entitiesSoA;
  eastl::vector<eastl::string> componentNames;
  eastl::vector<System> systems;
  eastl::vector<AsyncValue> asyncValues;
  eastl::vector<Query> queries;
  eastl::queue<CreateQueueData> createQueue;

  EventStream events;

  static void init();
  static void release();

  EntityManager();
  ~EntityManager();

  int getSystemId(const char *name) const;
  int getComponentNameId(const char *name) const;

  void addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names);

  void createEntity(const char *templ_name, const JValue &comps);
  void createEntitySync(const char *templ_name, const JValue &comps);
  void createEntitySyncSoA(const char *templ_name, const JValue &comps);

  void waitFor(EntityId eid, std::future<bool> && value);

  void invalidateQuery(Query &query);

  template <typename T>
  const T& getComponent(EntityId eid, const char *name)
  {
    const auto &e = entities[eid2idx(eid)];

    const auto &templ = templates[e.templateId];
    const auto &storage = storages[templ.storageId];

    const RegComp *desc = find_comp(Desc<T>::name);
    const int offset = templ.getCompontentOffset(desc->id, name);
    assert(offset >= 0);

    return *(T*)&storage.data[storage.size * e.memId + offset];
  }

  void tick();
  void tickStageSoA(int stage_id, const RawArg &stage);
  void sendEvent(EntityId eid, int event_id, const RawArg &ev);
  void sendEventSyncSoA(EntityId eid, int event_id, const RawArg &ev);

  template <typename S>
  void tickSoA(const S &stage)
  {
    RawArgSpec<sizeof(S)> arg0;
    new (arg0.mem) S(stage);

    tickStageSoA(RegCompSpec<S>::ID, arg0);
  }

  template <typename E>
  void sendEvent(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEvent(eid, RegCompSpec<E>::ID, arg0);
  }

  template <typename E>
  void sendEventSyncSoA(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventSyncSoA(eid, RegCompSpec<E>::ID, arg0);
  }

  // TODO: cache components descs
  // Query createQuery(...)

  /*template <typename SysT, typename C>
  void query(const C &callback, std::initializer_list<const char*> comp_names)
  {
    using T = RegSysSpec<SysT>;
    T::StringArray names;

    eastl::copy(comp_names.begin(), comp_names.end(), names.begin());

    T sys(eastl::move(eastl::function<SysT>(callback)), eastl::move(names));
    sys.init();

    RegSys::Remap remap;
    for (auto &e : entitiesSoA)
    {
      if (!e.ready)
        continue;

      const auto &templ = templates[e.templateId];

      bool ok = true;
      for (const auto &c : sys.components)
        if (c.desc->id != eidCompId && !templ.hasCompontent(c.desc->id, c.name.c_str()))
        {
          ok = false;
          break;
        }

      if (ok)
      {
        sys.initRemap(templ.components, remap);
        sys(e.eid, remap, &e.componentOffsets[0], &storagesSoA[0]);
      }
    }
  }*/

  void queryEids(EntityVector &out_eids, std::initializer_list<eastl::pair<const char*, const char*>> comps)
  {
    for (auto &e : entitiesSoA)
    {
      const auto &templ = templates[e.templateId];
      auto &storage = storages[templ.storageId];

      bool ok = true;
      for (const auto &c : comps)
      {
        const RegComp *desc = find_comp(c.first);
        if (!templ.hasCompontent(desc->id, c.second))
        {
          ok = false;
          break;
        }
      }

      if (ok)
        out_eids.push_back(e.eid);
    }
  }
};

extern EntityManager *g_mgr;