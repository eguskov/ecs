#include "ecs.h"

EntityManager *g_mgr = nullptr;

RegSys *reg_sys_head = nullptr;
int reg_sys_count = 0;

RegComp *reg_comp_head = nullptr;
int reg_comp_count = 0;

static inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId((uint32_t)gen << 16 | index);
}

static inline int eid2idx(EntityId eid)
{
  return eid.handle & 0xFFFF;
}

RegSys::RegSys(const char *_name, int _id) : id(_id)
{
  name = ::_strdup(_name);
}

RegSys::~RegSys()
{
  ::free(name);
}

bool RegSys::hasCompontent(int id, const char *name) const
{
  for (const auto &c : components)
    if (c.desc->id == id && c.name == name)
      return true;
  return false;
}

const RegSys *find_sys(const char *name)
{
  const RegSys *head = reg_sys_head;
  while (head)
  {
    if (::strcmp(head->name, name) == 0)
      return head;
    head = head->next;
  }
  return nullptr;
}

RegComp::RegComp(const char *_name, int _size) : id(reg_comp_count), size(_size)
{
  name = ::_strdup(_name);
  next = reg_comp_head;
  reg_comp_head = this;
  ++reg_comp_count;
}

RegComp::~RegComp()
{
  ::free(name);
}

const RegComp *find_comp(const char *name)
{
  const RegComp *head = reg_comp_head;
  while (head)
  {
    if (::strcmp(head->name, name) == 0)
      return head;
    head = head->next;
  }
  return nullptr;
}

bool Template::hasCompontent(int id, const char *name) const
{
  if (!name || !name[0])
    return compMask[id];
  for (const auto &c : components)
    if (c.desc->id == id && c.name == name)
      return true;
  return false;
}


void EventStream::push(EntityId eid, int event_id, const RawArg &ev)
{
  ++count;

  int sz = sizeof(eid) + sizeof(event_id) + sizeof(ev.size) + ev.size;
  int pos = 0;

  if (pushOffset + sz <= (int)data.size())
    pos = pushOffset;
  else
  {
    pos = pushOffset;
    data.resize(pushOffset + sz);
  }

  new (&data[pos]) EntityId(eid);
  pos += sizeof(eid);

  new (&data[pos]) int(event_id);
  pos += sizeof(event_id);

  new (&data[pos]) int(ev.size);
  pos += sizeof(ev.size);

  ::memcpy(&data[pos], ev.mem, ev.size);

  pushOffset += sz;
}

std::tuple<EntityId, int, RawArg> EventStream::pop()
{
  assert(count > 0);

  if (count <= 0)
    return {};

  EntityId eid = *(EntityId*)&data[popOffset];
  popOffset += sizeof(eid);

  int event_id = *(int*)&data[popOffset];
  popOffset += sizeof(event_id);

  int event_size = *(int*)&data[popOffset];
  popOffset += sizeof(event_size);

  uint8_t *mem = &data[popOffset];
  popOffset += event_size;

  if (--count == 0)
  {
    popOffset = 0;
    pushOffset = 0;
  }

  return { eid, event_id, RawArg{ event_size, mem } };
}

void EntityManager::init()
{
  if (g_mgr)
    return;

  g_mgr = new EntityManager;
}

EntityManager::EntityManager()
{
  // Init systems' descs
  {
    const RegSys *head = reg_sys_head;
    while (head)
    {
      const_cast<RegSys*>(head)->init();
      head = head->next;
    }
  }

  for (const RegSys *sys = reg_sys_head; sys; sys = sys->next)
    systems.push_back({ getSystemId(sys->name), sys });

  std::sort(systems.begin(), systems.end(),
    [](const System &lhs, const System &rhs) { return lhs.id < rhs.id; });

  // Add template
  addTemplate("test", { { "velocity", "vel" }, { "position", "pos" }, { "position", "pos1"} });
  addTemplate("test1", { { "test", "test" }, { "position", "pos1" } });

  eidCompId = find_comp("eid")->id;
}

int EntityManager::getSystemId(const char *name)
{
  if (::strcmp(name, "update_position") == 0) return 0;
  if (::strcmp(name, "position_printer") == 0) return 1;
  if (::strcmp(name, "test_printer") == 0) return 2;
  return 0;
}

void EntityManager::addTemplate(const char *templ_name, std::initializer_list<std::pair<const char*, const char*>> compNames)
{
  templates.emplace_back();
  auto &templ = templates.back();
  templ.name = templ_name;
  templ.compMask.resize(reg_comp_count);
  templ.compMask.assign(reg_comp_count, false);

  for (const auto &name : compNames)
    templ.components.push_back({ 0, name.second, find_comp(name.first) });

  std::sort(templ.components.begin(), templ.components.end(),
    [](const CompDesc &lhs, const CompDesc &rhs)
    {
      if (lhs.desc->id == rhs.desc->id)
        return lhs.name < rhs.name;
      return lhs.desc->id < rhs.desc->id;
    });

  int offset = 0;
  for (auto &c : templ.components)
  {
    c.offset = offset;
    offset += c.desc->size;

    templ.compMask[c.desc->id] = true;
  }
  templ.size = offset;

  for (size_t i = 0; i < storages.size(); ++i)
    if (storages[i].size == templ.size)
    {
      templ.storageId = (int)i;
      break;
    }

  if (templ.storageId < 0)
  {
    storages.emplace_back();
    auto &s = storages.back();
    s.size = templ.size;
    templ.storageId = (int)storages.size() - 1;
  }
}

EntityId EntityManager::createEntity(const char *templ_name)
{
  int templateId = -1;
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == templ_name)
    {
      templateId = (int)i;
      break;
    }

  // TOOD: Search template by name
  auto &templ = templates[templateId];
  auto &storage = storages[templ.storageId];

  uint8_t *mem = storage.allocate();

  for (const auto &c : templ.components)
    c.desc->init(mem + c.offset);

  entities.emplace_back();
  auto &e = entities.back();
  e.eid = make_eid(1, (uint16_t)entities.size() - 1);
  e.templateId = templateId;
  e.memId = storage.count - 1;

  return e.eid;
}

void EntityManager::tick()
{
  while (events.count)
  {
    EntityId eid;
    int event_id = -1;
    RawArg ev;
    std::tie(eid, event_id, ev) = events.pop();
    processEvent(eid, event_id, ev);
  }
}

void EntityManager::tickStage(int stage_id, const RawArg &stage)
{
  for (const auto &sys : systems)
  {
    if (sys.desc->stageId != stage_id)
      continue;

    for (auto &e : entities)
    {
      const auto &templ = templates[e.templateId];
      auto &storage = storages[templ.storageId];

      bool ok = true;
      for (const auto &c : sys.desc->components)
        if (c.desc->id != eidCompId && c.desc->id != stage_id && !templ.hasCompontent(c.desc->id, c.name.c_str()))
        {
          ok = false;
          break;
        }

      if (ok)
      {
        RawArgSpec<sizeof(EntityId)> eid;
        new (eid.mem) EntityId(e.eid);

        sys.desc->stageFn(stage, eid, templ.components, &storage.data[storage.size * e.memId]);
      }
    }
  }
}

void EntityManager::sendEvent(EntityId eid, int event_id, const RawArg &ev)
{
  events.push(eid, event_id, ev);
}

void EntityManager::processEvent(EntityId eid, int event_id, const RawArg &ev)
{
  auto &e = entities[eid2idx(eid)];
  const auto &templ = templates[e.templateId];
  auto &storage = storages[templ.storageId];

  for (const auto &sys : systems)
    if (sys.desc->eventId == event_id)
    {
      bool ok = true;
      for (const auto &c : sys.desc->components)
        if (c.desc->id != eidCompId && c.desc->id != event_id && !templ.hasCompontent(c.desc->id, c.name.c_str()))
        {
          ok = false;
          break;
        }

      if (ok)
      {
        RawArgSpec<sizeof(EntityId)> eid;
        new (eid.mem) EntityId(e.eid);

        sys.desc->stageFn(ev, eid, templ.components, &storage.data[storage.size * e.memId]);
      }
    }
}
