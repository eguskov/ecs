#pragma once

#include "stdafx.h"

#include <future>
#include <EASTL/deque.h>

#include "io/json.h"

#include "entity.h"
#include "component.h"
#include "query.h"
#include "system.h"
#include "index.h"

#include "event.h"
#include "stage.h"

#include "components/core.h"

#include "jobmanager.h"

#define PULL_ESC_CORE \
  extern const uint32_t ecs_pull_core; \
  uint32_t ecs_pull = 0 + ecs_pull_core;

using FrameSnapshot = eastl::vector<uint8_t*, FrameMemAllocator>;

struct EventOnEntityCreate
{
};

ECS_EVENT(EventOnEntityCreate);

struct EventOnEntityReady
{
};

ECS_EVENT(EventOnEntityReady);

struct EventOnChangeDetected
{
};

ECS_EVENT(EventOnChangeDetected);

struct EntityTemplate
{
  int size = 0;
  int docId = -1;
  int archetypeId = -1;

  eastl::string name;
  eastl::vector<Component> components;
  eastl::vector<int> extends;
};

template <typename T>
struct CleanupType
{
  using Type = typename eastl::remove_const<typename eastl::remove_reference<T>::type>::type;
};

struct Entity
{
  bool ready = false;

  int templateId = -1;
  int archetypeId = -1;
  int indexInArchetype = -1;
};

struct System
{
  int weight;
  SystemDescription::SystemCallback sys;
  const SystemDescription *desc;
};

struct EventStream
{
  enum Flags
  {
    kUnicast = 0,
    kBroadcast = 1 << 0,
  };

  struct Header
  {
    EntityId eid;
    uint8_t flags;
    int eventId;
    int eventSize;
  };

  int popOffset = 0;
  int pushOffset = 0;
  int count = 0;
  eastl::vector<uint8_t> data;

  void push(EntityId eid, uint8_t flags, int event_id, const RawArg &ev);
  eastl::tuple<Header, RawArg> pop();
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

struct Archetype
{
  int entitiesCount = 0;
  int entitiesCapacity = 0;
  eastl::vector<Storage*> storages;
  eastl::vector<HashedString> storageNames;

  ~Archetype()
  {
  }

  void invalidate()
  {
    for (auto &s : storages)
      s->invalidate();
  }

  void clear()
  {
    for (Storage *s : storages)
      delete s;

    storages.clear();
    storageNames.clear();
  }

  bool hasCompontent(const HashedString &name) const;
  bool hasCompontent(const ConstHashedString &name) const;

  int getComponentIndex(const HashedString &name) const;
  int getComponentIndex(const ConstHashedString &name) const;
};

struct EntityManager
{
  int eidCompId = -1;
  const ComponentDescription *eidComp = nullptr;

  JDocument templatesDoc;

  eastl::vector<eastl::string> order;
  eastl::vector<EntityTemplate> templates;
  eastl::vector<Archetype> archetypes;
  int entitiesCount = 0;
  eastl::vector<Entity> entities;
  eastl::vector<uint8_t> entityGenerations;
  eastl::hash_map<HashedString, const ComponentDescription*> componentDescByNames;
  eastl::vector<System> systems;
  eastl::hash_multimap<uint32_t, int> systemsByStage;
  eastl::vector<const SystemDescription*> systemDescs;
  eastl::vector<eastl::vector<int>> systemDependencies;
  eastl::vector<jobmanager::JobId> systemJobs;
  eastl::vector<AsyncValue> asyncValues;
  eastl::vector<Query> queries;
  eastl::vector<Query> namedQueries;
  eastl::vector<Index> namedIndices;
  eastl::vector<int> dirtyQueries;
  eastl::vector<int> dirtyNamedQueries;
  eastl::vector<int> dirtyNamedIndices;

  eastl::queue<CreateQueueData> createQueue;
  eastl::queue<EntityId> deleteQueue;
  eastl::deque<uint32_t> freeEntityQueue;

  eastl::set<HashedString> trackComponents;

  int currentEventStream = 0;
  eastl::array<EventStream, 2> events;

  static void create();
  static void release();

  EntityManager();
  ~EntityManager();

  void init();

  int getSystemId(const ConstHashedString &name) const;
  jobmanager::DependencyList getSystemDependencyList(int id) const;
  void waitSystemDependencies(int id) const;

  int getSystemWeight(const ConstHashedString &name) const;
  const ComponentDescription* getComponentDescByName(const char *name) const;
  const ComponentDescription* getComponentDescByName(const HashedString &name) const;
  const ComponentDescription* getComponentDescByName(const ConstHashedString &name) const;

  Query* findQuery(const ConstHashedString &name);
  Index* findIndex(const ConstHashedString &name);

  int getTemplateId(const char *name);
  void addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names, const eastl::vector<const char*> &extends);
  void buildComponentsValuesFromTemplate(int template_id, const JValue &comps, JFrameValue &out_value);

  void findArchetypes(QueryDescription &desc);

  // TODO: Return EntityId from createEntity
  void createEntity(const char *templ_name, const JValue &comps);
  void createEntity(const char *templ_name, const JFrameValue &comps);
  EntityId createEntitySync(const char *templ_name, const JValue &comps);

  void deleteEntity(const EntityId &eid);

  void waitFor(EntityId eid, std::future<bool> && value);

  void performQuery(Query &query);
  void rebuildIndex(Index &index);
  void updateIndex(Index &index);

  void enableChangeDetection(const HashedString &name);
  void disableChangeDetection(const HashedString &name);

  void fillFrameSnapshot(FrameSnapshot &snapshot) const;
  void checkFrameSnapshot(const FrameSnapshot &snapshot);

  void tick();
  void tickStage(uint32_t stage_id, const RawArg &stage);
  void sendEvent(EntityId eid, uint32_t event_id, const RawArg &ev);
  void sendEventSync(EntityId eid, uint32_t event_id, const RawArg &ev);

  void sendEventBroadcast(uint32_t event_id, const RawArg &ev);
  void sendEventBroadcastSync(uint32_t event_id, const RawArg &ev);

  template <typename S>
  void tick(const S &stage)
  {
    RawArgSpec<sizeof(S)> arg0;
    new (arg0.mem) S(stage);

    tickStage(StageType<S>::id, arg0);
  }

  template <typename E>
  void sendEvent(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEvent(eid, EventType<E>::id, arg0);
  }

  template <typename E>
  void sendEventSync(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventSync(eid, EventType<E>::id, arg0);
  }

  template <typename E>
  void sendEventBroadcast(const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventBroadcast(EventType<E>::id, arg0);
  }

  template <typename E>
  void sendEventBroadcastSync(const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventBroadcastSync(EventType<E>::id, arg0);
  }
};

extern EntityManager *g_mgr;

namespace ecs
{
  inline void init() { EntityManager::create(); }
  inline void release() { EntityManager::release(); }

  inline void tick() { g_mgr->tick(); }

  template <typename S> inline void tick(const S &stage) { g_mgr->tick<S>(stage); }

  inline void create_entity(const char *templ_name, const JValue &comps) { g_mgr->createEntity(templ_name, comps); }
  inline void create_entity(const char *templ_name, const JFrameValue &comps) { g_mgr->createEntity(templ_name, comps); }
  inline EntityId create_entity_sync(const char *templ_name, const JValue &comps) { return g_mgr->createEntitySync(templ_name, comps); }
  inline void delete_entity(const EntityId &eid) { g_mgr->deleteEntity(eid); }

  inline void perform_query(Query &query) { g_mgr->performQuery(query); }
  inline void perform_query(const QueryDescription &desc, Query &query)
  {
    query.desc = desc;
    g_mgr->findArchetypes(query.desc);
    g_mgr->performQuery(query);
  }
  inline void perform_query(const ConstQueryDescription &desc, Query &query)
  {
    query.desc = desc;
    g_mgr->findArchetypes(query.desc);
    g_mgr->performQuery(query);
  }
  inline Query perform_query(const QueryDescription &desc)
  {
    Query query;
    query.desc = desc;
    g_mgr->findArchetypes(query.desc);
    g_mgr->performQuery(query);
    return query;
  }
  inline Query perform_query(const ConstQueryDescription &desc)
  {
    Query query;
    query.desc = desc;
    g_mgr->findArchetypes(query.desc);
    g_mgr->performQuery(query);
    return query;
  }

  template <typename E> inline void send_event(EntityId eid, const E &ev) { g_mgr->sendEvent<E>(eid, ev); }
  template <typename E> inline void send_event_broadcast(const E &ev) { g_mgr->sendEventBroadcast<E>(ev); }

  inline Query* find_query(const ConstHashedString &name) { return g_mgr->findQuery(name); }
  inline Index* find_index(const ConstHashedString &name) { return g_mgr->findIndex(name); }

  inline int get_system_id(const ConstHashedString &name) { return g_mgr->getSystemId(name); }
  inline jobmanager::DependencyList get_system_dependency_list(int id)  { return g_mgr->getSystemDependencyList(id); }

  inline void wait_system_dependencies(const ConstHashedString &name) { g_mgr->waitSystemDependencies(ecs::get_system_id(name)); }
  inline void wait_system_dependencies(int system_id) { g_mgr->waitSystemDependencies(system_id); }

  inline void set_system_job(int system_id, const jobmanager::JobId &jid) { g_mgr->systemJobs[system_id] = jid; }
} //ecs