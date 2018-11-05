#pragma once

#include "stdafx.h"

#include <future>

#include "io/json.h"

#include "entity.h"
#include "component.h"
#include "query.h"
#include "system.h"

#include "event.h"
#include "stage.h"

#include "components/core.h"

#define ECS_PULL(type) static int pull_##type = RegCompSpec<type>::ID;
#define PULL_ESC_CORE ECS_PULL(bool);

using FrameSnapshot = eastl::vector<uint8_t*, FrameMemAllocator>;

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

struct EventOnChangeDetected : Event
{
};
REG_EVENT(EventOnChangeDetected);

struct EntityTemplate
{
  int size = 0;
  int docId = -1;
  int archetypeId = -1;

  eastl::string name;
  eastl::bitvector<> compMask;
  eastl::vector<CompDesc> components;
  eastl::vector<RegSys::Remap> remaps;
  eastl::vector<int> extends;

  bool hasCompontent(int type_id, int name_id) const;
  int getCompontentOffset(int type_id, int name_id) const;
};

template <typename T>
struct CleanupType
{
  using Type = typename eastl::remove_const<typename eastl::remove_reference<T>::type>::type;
};

struct Entity
{
  bool ready = false;

  // TODO: Is this needed here??
  EntityId eid;
  int templateId = -1;
  int archetypeId = -1;

  int indexInArchetype = -1;

  // TODO: Store as SoA in one array???
  // eastl::vector<int> componentOffsets;
};

struct EntityDesc
{
  eastl::hash_map<int, int> offsetsByNameId;
};

struct System
{
  int weight;
  const RegSys *desc;
};

struct EventStream
{
  enum Flags
  {
    kTarget = 0,
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
  struct StorageDesc
  {
    HashedString name;
    Storage *storage = nullptr;

    operator Storage*() { return storage; }
    Storage* operator->() { return storage; }
  };

  int entitiesCount = 0;
  // TODO: Remove hash_map!
  // eastl::hash_map<HashedString, Storage*> storageMap;
  // TODO: Inplace allocation for Storage. In order to reduce memory hops.
  eastl::vector<StorageDesc> storages;

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
  }

  bool hasCompontent(const HashedString &name) const;
  bool hasCompontent(const ConstHashedString &name) const;

  int getComponentIndex(const HashedString &name) const;
  int getComponentIndex(const ConstHashedString &name) const;
};

struct EntityManager
{
  int eidCompId = -1;
  const RegComp *eidComp = nullptr;

  JDocument templatesDoc;

  eastl::vector<eastl::string> order;
  eastl::vector<EntityTemplate> templates;
  eastl::vector<Archetype> archetypes;
  // eastl::vector<Storage*> storages;
  eastl::vector<Entity> entities;
  // eastl::vector<EntityDesc> entityDescs;
  eastl::vector<eastl::string> componentNames;
  eastl::vector<const RegComp*> componentDescByNames;
  eastl::vector<System> systems;
  eastl::vector<AsyncValue> asyncValues;
  eastl::vector<Query> queries;
  eastl::vector<QueryDesc> queryDescs;

  eastl::queue<CreateQueueData> createQueue;
  eastl::queue<EntityId> deleteQueue;
  eastl::queue<int> freeEntityQueue;

  eastl::set<HashedString> trackComponents;

  int currentEventStream = 0;
  eastl::array<EventStream, 2> events;

  static void create();
  static void release();

  EntityManager();
  ~EntityManager();

  void init();

  int getSystemWeight(const char *name) const;
  int getComponentNameId(const char *name) const;
  const char* getComponentName(int name_id) const;
  const RegComp* getComponentDescByName(const char *name) const;

  int getTemplateId(const char *name);
  void addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names, const eastl::vector<const char*> &extends);

  void createEntity(const char *templ_name, const JValue &comps);
  void createEntity(const char *templ_name, const JFrameValue &comps);
  EntityId createEntitySync(const char *templ_name, const JValue &comps);

  void deleteEntity(const EntityId &eid);

  void waitFor(EntityId eid, std::future<bool> && value);

  void invalidateQuery(Query &query);

  void enableChangeDetection(const HashedString &name);
  void disableChangeDetection(const HashedString &name);

  void fillFrameSnapshot(FrameSnapshot &snapshot) const;
  void checkFrameSnapshot(const FrameSnapshot &snapshot);

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
  void tickStage(int stage_id, const RawArg &stage);
  void sendEvent(EntityId eid, int event_id, const RawArg &ev);
  void sendEventSync(EntityId eid, int event_id, const RawArg &ev);

  void sendEventBroadcast(int event_id, const RawArg &ev);
  void sendEventBroadcastSync(int event_id, const RawArg &ev);

  template <typename S>
  void tick(const S &stage)
  {
    RawArgSpec<sizeof(S)> arg0;
    new (arg0.mem) S(stage);

    tickStage(RegCompSpec<S>::ID, arg0);
  }

  template <typename E>
  void sendEvent(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEvent(eid, RegCompSpec<E>::ID, arg0);
  }

  template <typename E>
  void sendEventSync(EntityId eid, const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventSync(eid, RegCompSpec<E>::ID, arg0);
  }

  template <typename E>
  void sendEventBroadcast(const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventBroadcast(RegCompSpec<E>::ID, arg0);
  }

  template <typename E>
  void sendEventBroadcastSync(const E &ev)
  {
    RawArgSpec<sizeof(E)> arg0;
    new (arg0.mem) E(ev);

    sendEventBroadcastSync(RegCompSpec<E>::ID, arg0);
  }
};

extern EntityManager *g_mgr;