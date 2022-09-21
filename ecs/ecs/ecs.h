#pragma once

#include "stdafx.h"

#include <future>
#include <EASTL/deque.h>
#include <EASTL/unique_ptr.h>

#include "entity.h"
#include "component.h"
#include "query.h"
#include "system.h"
#include "index.h"

#include "event.h"
#include "ecs-events.h"

#include "components/core.h"

#include "jobmanager.h"

#include "framemem.h"

#include "debug.h"

#define PULL_ESC_CORE \
  extern uint32_t ecs_pull_core; \
  extern uint32_t ecs_events_h_pull; \
  uint32_t ecs_pull = 0 + ecs_pull_core + ecs_events_h_pull;

using FrameSnapshot = eastl::vector<uint8_t*, FrameMemAllocator>;

template <typename T>
struct CleanupType
{
  using Type = typename eastl::remove_const<typename eastl::remove_reference<T>::type>::type;
};

struct ComponentsMap
{
  struct Offset
  {
    uint16_t chunkId;
    uint16_t offsetInChunk;
  };

  struct Value
  {
    const ComponentDescription *desc;
    Offset offset;
  };

  eastl::hash_map<HashedString, Value> components;

  static constexpr uint32_t MAX_CHUNK_SIZE = 1024;

  using Chunk = eastl::vector<uint8_t>;

  eastl::vector<Chunk> chunks;

  int topChunkId = -1;

  ComponentsMap() = default;
  ComponentsMap(const ComponentsMap&) = default;
  ComponentsMap(ComponentsMap &&) = default;

  ComponentsMap& operator=(const ComponentsMap&) = default;
  ComponentsMap& operator=(ComponentsMap &&) = default;

  inline uint8_t *get(const Offset &offset)
  {
    return &chunks[offset.chunkId][offset.offsetInChunk];
  }

  inline const uint8_t *get(const Offset &offset) const
  {
    return &chunks[offset.chunkId][offset.offsetInChunk];
  }

  Offset allocate(uint32_t sz)
  {
    if (topChunkId < 0 || chunks[topChunkId].size() + sz > MAX_CHUNK_SIZE)
    {
      topChunkId = (int)chunks.size();
      chunks.emplace_back();
      chunks[topChunkId].reserve(MAX_CHUNK_SIZE);
    }

    uint32_t offset = (uint32_t)chunks[topChunkId].size();
    chunks[topChunkId].resize(offset + sz);

    return { (uint16_t)(chunks.size() - 1), (uint16_t)(offset) };
  }

  uint8_t* createComponent(const HashedString &name, const ComponentDescription *desc)
  {
    auto res = components.find(name);
    if (res != components.end())
    {
      ASSERT(res->second.desc == desc);
      return get(res->second.offset);
    }

    Offset offset = allocate(desc->size);
    desc->ctor(get(offset));

    auto it = components.insert(name);
    it.first->second = {desc, offset};

    return get(offset);
  }

  inline uint8_t* createComponent(const ConstHashedString &name, const ComponentDescription *desc)
  {
    return createComponent(HashedString(name), desc);
  }

  inline uint8_t* createComponent(const char *name, const ComponentDescription *desc)
  {
    return createComponent(HashedString(name), desc);
  }

  template<typename T>
  void add(const ConstHashedString &name, const T &val)
  {
    const auto *desc = g_mgr->getComponentDescByName(name);
    ASSERT(desc != nullptr);

    *(T*)createComponent(name, desc) = val;
  }

  template<typename T>
  void add(const ConstHashedString &name, T &&val)
  {
    const auto *desc = g_mgr->getComponentDescByName(name);
    ASSERT(desc != nullptr);

    *(CleanupType<T>::Type*)createComponent(name, desc) = eastl::move(val);
  }
};

struct EntityTemplate
{
  int size = 0;
  int archetypeId = -1;

  eastl::string name;
  ComponentsMap cmap;

  EntityTemplate() = default;
  EntityTemplate(const EntityTemplate&) = default;
  EntityTemplate(EntityTemplate &&) = default;

  EntityTemplate& operator=(const EntityTemplate&) = default;
  EntityTemplate& operator=(EntityTemplate &&) = default;
};

// TODO: Rename. This is not an actual Entity
struct Entity
{
  bool ready = false;

  int32_t templateId = -1;
  int32_t archetypeId = -1;
  int32_t indexInArchetype = -1;
};

// TODO: inherit from SystemDescription
struct System
{
  SystemDescription::SystemCallback sys;
  const SystemDescription *desc;

  HashedString name;

  SystemId id;
  QueryId queryId;

  void reset();
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
  ComponentsMap components;

  ECS_DEFAULT_CTORS(CreateQueueData);
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
  struct Storage
  {
    const ComponentDescription *desc;

    uint8_t *items = nullptr;
    int32_t itemSize  = 0;
    int32_t totalSize = 0;

    Storage(const Storage &) = delete;
    Storage(Storage &&) = delete;

    Storage& operator=(const Storage &) = delete;
    Storage& operator=(Storage &&) = delete;

    Storage() = default;
    Storage(const ComponentDescription *_desc) : desc(_desc), itemSize(desc->size) {}

    void clear(const eastl::bitvector<> &free_mask, int32_t count)
    {
      if (!items)
        return;

      for (int i = 0; i < count; ++i)
        if (!free_mask[i])
          desc->dtor(items + i * itemSize);

      delete[] items;
      items = nullptr;
    }

    void resize(const eastl::bitvector<> &free_mask, int32_t old_count, int32_t new_count)
    {
      totalSize = new_count * itemSize;

      // TODO: What about alignment ???
      uint8_t *newItems = new uint8_t[totalSize];
      ::memset(newItems, 0, totalSize);

      // TODO: Move only if component's type non memcpy-only
      for (int32_t i = 0; i < old_count; ++i)
        if (!free_mask[i])
          desc->move(newItems + i * itemSize, items + i * itemSize);

      delete[] items;
      items = newItems;
    }

    inline void ctor(int32_t index, const uint8_t *val)
    {
      desc->ctor(items + index * itemSize);
      desc->copy(items + index * itemSize, val);
    }

    inline void dtor(int32_t index) { desc->dtor(items + index * itemSize); }

    inline uint8_t* get(int32_t index) { return items + (index * itemSize); }
    inline void set(int32_t index, const uint8_t *val) { desc->copy(items + index * itemSize, val); }

    inline size_t size() const { return totalSize; }
  };

  int32_t entitiesCount = 0;
  int32_t entitiesCapacity = 0;
  int32_t componentsCount = 0;

  eastl::unique_ptr<Storage[]>      storages;
  eastl::unique_ptr<HashedString[]> storageNames;

  eastl::bitvector<> freeMask;
  eastl::deque<int32_t> freeIndexQueue;

  void clear()
  {
    for (int i = 0; i < componentsCount; ++i)
      storages[i].clear(freeMask, entitiesCapacity);

    storages.reset();
    storageNames.reset();
  }

  bool hasCompontent(const HashedString &name) const;
  bool hasCompontent(const ConstHashedString &name) const;

  int getComponentIndex(const HashedString &name) const;
  int getComponentIndex(const ConstHashedString &name) const;

  inline int32_t popFreeIndex()
  {
    if (freeIndexQueue.empty())
      return -1;
    const int32_t i = freeIndexQueue.front();
    freeIndexQueue.pop_front();
    return i;
  }

  int32_t allocate(const ComponentsMap &init, ComponentsMap &&cmap)
  {
    ASSERT(init.components.size() == componentsCount);

    entitiesCount++;

    int32_t entityIndex = -1;

    const int32_t freeIndex = popFreeIndex();
    if (freeIndex >= 0)
    {
      ASSERT(freeMask[freeIndex] == true);
      entityIndex = freeIndex;
    }
    else
    {
      // TODO: Allocate more tha 1 entity
      entityIndex = entitiesCapacity;
      ++entitiesCapacity;

      freeMask.resize(entitiesCapacity);

      for (int i = 0; i < componentsCount; ++i)
        storages[i].resize(freeMask, entitiesCapacity - 1, entitiesCapacity);
    }

    freeMask[entityIndex] = false;

    int i = 0;
    for (const auto &kv : init.components)
    {
      ASSERT(storages[i].desc == kv.second.desc);
      storages[i++].ctor(entityIndex, init.get(kv.second.offset));
    }

    for (const auto &kv : cmap.components)
    {
      const int32_t compIdx = getComponentIndex(kv.first);
      if (compIdx >= 0)
        storages[compIdx].set(entityIndex, cmap.get(kv.second.offset));
    }

    return entityIndex;
  }

  void deallocate(int32_t entity_index)
  {
    ASSERT(entity_index >= 0 && entity_index < entitiesCapacity);
    ASSERT(freeMask[entity_index] == false);

    freeIndexQueue.push_back(entity_index);
    freeMask[entity_index] = true;

    --entitiesCount;
    for (int i = 0; i < componentsCount; ++i)
      storages[i].dtor(entity_index);
  }

  inline uint8_t* getRaw(int32_t entity_index, int32_t i)
  {
    ASSERT(entity_index >= 0 && entity_index < entitiesCapacity);
    ASSERT(i >= 0 && i < componentsCount);
    ASSERT(freeMask[entity_index] == false);
    return storages[i].get(entity_index);
  }

  inline const uint8_t* getRaw(int32_t entity_index, int32_t i) const
  {
    ASSERT(entity_index >= 0 && entity_index < entitiesCapacity);
    ASSERT(i >= 0 && i < componentsCount);
    ASSERT(freeMask[entity_index] == false);
    return storages[i].get(entity_index);
  }

  template <typename T>
  inline const T& get(int32_t entity_index, int32_t i) const
  {
    ASSERT(entity_index >= 0 && entity_index < entitiesCapacity);
    ASSERT(i >= 0 && i < componentsCount);
    ASSERT(ComponentType<T>::type == storages[i].desc->typeHash);
    return *(T*)getRaw(entity_index, i);
  }

  template <typename T>
  inline T& get(int32_t entity_index, int32_t i)
  {
    ASSERT(entity_index >= 0 && entity_index < entitiesCapacity);
    ASSERT(i >= 0 && i < componentsCount);
    // TODO: Fix this. Index causes as assert here!
    // ASSERT(ComponentType<T>::type == storages[i].desc->typeHash);
    return *(T*)getRaw(entity_index, i);
  }
};

struct EntityManager
{
  eastl::vector<eastl::string> order;
  eastl::vector<EntityTemplate> templates;
  eastl::vector<Archetype> archetypes;
  int entitiesCount = 0;
  HandleFactory<EntityId, 1024> eidFactory;
  eastl::vector<Entity> entities;
  eastl::hash_map<HashedString, const ComponentDescription*> componentDescByNames;
  HandleFactory<SystemId, 1024> sidFactory;
  eastl::vector<System> systems;
  eastl::vector<SystemId> systemsSorted;
  eastl::hash_map<uint32_t, eastl::vector<SystemId>> systemsByStage;
  eastl::hash_map<HashedString, SystemId> systemsByName;
  eastl::vector<eastl::vector<SystemId>> systemDependencies;
  eastl::vector<jobmanager::JobId> systemJobs;
  eastl::vector<AsyncValue> asyncValues;
  eastl::vector<Index> namedIndices;
  eastl::vector<QueryId> dirtyQueries;
  eastl::vector<int> dirtyNamedIndices;

  HandleFactory<QueryId, 1024> qidFactory;
  eastl::vector<Query> queries;
  eastl::vector<QueryDescription> queryDescriptions;

  eastl::queue<CreateQueueData> createQueue;
  eastl::queue<EntityId> deleteQueue;

  eastl::set<HashedString> trackComponents;

  int currentEventStream = 0;
  eastl::array<EventStream, 2> events;

  bool isDirtySystems = false;

  static void create();
  static void release();

  EntityManager();
  ~EntityManager();

  void init();
  void sortSystems();
  void buildSystemsDependencies();

  SystemId getSystemId(const ConstHashedString &name) const;
  jobmanager::DependencyList getSystemDependencyList(SystemId sid) const;
  void waitSystemDependencies(SystemId sid) const;

  const ComponentDescription* getComponentDescByName(const char *name) const;
  const ComponentDescription* getComponentDescByName(const HashedString &name) const;
  const ComponentDescription* getComponentDescByName(const ConstHashedString &name) const;

  Index* findIndex(const ConstHashedString &name);

  int getTemplateId(const char *name);
  void addTemplate(const char *templ_name, ComponentsMap &&cmap);

  void findArchetypes(QueryDescription &desc);

  // TODO: Return EntityId from createEntity
  void createEntity(const char *templ_name, ComponentsMap &&comps);
  EntityId createEntitySync(const char *templ_name, ComponentsMap &&comps);

  void deleteEntity(const EntityId &eid);

  void waitFor(EntityId eid, std::future<bool> && value);

  inline Query& getQuery(const QueryId &qid) { ASSERT(qidFactory.isValid(qid)); return queries[qid.index]; }
  inline int32_t getEntitiesCount(const QueryId &qid) const { return qidFactory.isValid(qid) ? queries[qid.index].entitiesCount : 0; }

  void performQuery(const QueryId &qid);
  void performQuery(const QueryDescription &desc, Query &query);
  void rebuildIndex(Index &index);
  void updateIndex(Index &index);

  void enableChangeDetection(const HashedString &name);
  void disableChangeDetection(const HashedString &name);

  void fillFrameSnapshot(FrameSnapshot &snapshot) const;
  void checkFrameSnapshot(const FrameSnapshot &snapshot);

  void tick();
  void sendEvent(EntityId eid, uint32_t event_id, const RawArg &ev);
  void sendEventSync(EntityId eid, uint32_t event_id, const RawArg &ev);

  void sendEventBroadcast(uint32_t event_id, const RawArg &ev);
  void sendEventBroadcastSync(uint32_t event_id, const RawArg &ev);
  void invokeEventBroadcast(uint32_t event_id, const RawArg &ev);

  SystemId createSystem(const HashedString &name, const SystemDescription *desc, const QueryDescription *query_desc = nullptr);
  void deleteSystem(const SystemId &sid);

  QueryId createQuery(const HashedString &name, const QueryDescription &desc, const filter_t &filter = nullptr);
  QueryId createQuery(const HashedString &name, const ConstQueryDescription &desc, const filter_t &filter = nullptr);
  void deleteQuery(const QueryId &qid);

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

  template <typename E>
  void invokeEventBroadcast(const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    invokeEventBroadcast(EventType<E>::id, arg0);
  }
};

extern EntityManager *g_mgr;

namespace ecs
{
  inline void init() { EntityManager::create(); }
  inline void release() { EntityManager::release(); }

  inline void tick() { g_mgr->tick(); }

  inline void create_entity(const char *templ_name, ComponentsMap &&comps) { g_mgr->createEntity(templ_name, eastl::move(comps)); }
  inline EntityId create_entity_sync(const char *templ_name, ComponentsMap &&comps) { return g_mgr->createEntitySync(templ_name, eastl::move(comps)); }
  inline void delete_entity(const EntityId &eid) { g_mgr->deleteEntity(eid); }

  inline int32_t get_entities_count(const QueryId &query_id) { return g_mgr->getEntitiesCount(query_id); }
  inline Query& get_query(const QueryId &query_id) { return g_mgr->getQuery(query_id); }

  inline void perform_query(const QueryId &query_id) { g_mgr->performQuery(query_id); }

  inline void perform_query(QueryDescription &&desc, Query &query) { g_mgr->findArchetypes(desc); g_mgr->performQuery(desc, query); }
  inline Query perform_query(QueryDescription &&desc) { Query query; perform_query(eastl::move(desc), query); return query; }

  inline void perform_query(const ConstQueryDescription &in_desc, Query &query) { perform_query(QueryDescription(in_desc), query); }
  inline Query perform_query(const ConstQueryDescription &in_desc) { Query query; perform_query(QueryDescription(in_desc), query); return query; }

  template <typename E> inline void send_event(EntityId eid, const E &ev) { g_mgr->sendEvent<E>(eid, ev); }
  template <typename E> inline void send_event_broadcast(const E &ev) { g_mgr->sendEventBroadcast<E>(ev); }
  template <typename E> inline void invoke_event_broadcast(const E &ev) { g_mgr->invokeEventBroadcast<E>(ev); }

  inline Index* find_index(const ConstHashedString &name) { return g_mgr->findIndex(name); }

  inline SystemId get_system_id(const ConstHashedString &name) { return g_mgr->getSystemId(name); }
  inline jobmanager::DependencyList get_system_dependency_list(SystemId sid)  { return g_mgr->getSystemDependencyList(sid); }

  inline void wait_system_dependencies(const ConstHashedString &name) { g_mgr->waitSystemDependencies(ecs::get_system_id(name)); }
  inline void wait_system_dependencies(SystemId sid) { g_mgr->waitSystemDependencies(sid); }

  inline void set_system_job(SystemId sid, const jobmanager::JobId &jid)
  {
    if (g_mgr->sidFactory.isValid(sid))
      g_mgr->systemJobs[sid.index] = jid;
  }
} //ecs