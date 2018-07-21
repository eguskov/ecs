#pragma once

#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>

#include "io/json.h"

#include "entity.h"
#include "component.h"
#include "system.h"

#include "event.h"

#include "components/eid.component.h"

struct RegSys;
extern RegSys *reg_sys_head;
extern int reg_sys_count;

struct RegComp;
extern RegComp *reg_comp_head;
extern int reg_comp_count;

struct Template
{
  int size = 0;
  int storageId = -1;
  int docId = -1;

  std::string name;
  std::bitarray compMask;
  std::vector<CompDesc> components;

  bool hasCompontent(int id, const char *name) const;
};

template <typename T>
struct Cleanup
{
  using Type = typename std::remove_const<typename std::remove_reference<T>::type>::type;
};

struct Storage
{
  int count = 0;
  int size = 0;

  std::vector<uint8_t> data;

  uint8_t* allocate()
  {
    ++count;
    data.resize(count * size);
    return &data[(count - 1) * size];
  }
};

struct Entity
{
  EntityId eid;
  int templateId = -1;
  int memId = -1;
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
  std::vector<uint8_t> data;

  void push(EntityId eid, int event_id, const RawArg &ev);
  std::tuple<EntityId, int, RawArg> pop();
};

struct EntityManager
{
  int eidCompId = -1;

  JDocument templatesDoc;

  std::vector<std::string> order;
  std::vector<Template> templates;
  std::vector<Storage> storages;
  std::vector<Entity> entities;
  std::vector<System> systems;

  EventStream events;

  static void init();

  EntityManager();

  int getSystemId(const char *name);

  void addTemplate(int doc_id, const char *templ_name, const std::vector<std::pair<const char*, const char*>> &comp_names);

  EntityId createEntity(const char *templ_name, const JValue &comps);

  void tick();
  void tickStage(int stage_id, const RawArg &stage);
  void sendEvent(EntityId eid, int event_id, const RawArg &ev);
  void processEvent(EntityId eid, int event_id, const RawArg &ev);

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
};

extern EntityManager *g_mgr;