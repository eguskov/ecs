#include "ecs.h"

RegSys *reg_sys_head = nullptr;
int reg_sys_count = 0;

RegComp *reg_comp_head = nullptr;
int reg_comp_count = 0;

static RegCompSpec<EntityId> _eid;
static RegCompSpec<UpdateStage> _update_stage;
int RegCompSpec<UpdateStage>::ID = -1;

static inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId((uint32_t)gen << 16 | index);
}

RegSys::RegSys(const char *_name, int _id) : id(_id)
{
  name = ::_strdup(_name);
}

RegSys::~RegSys()
{
  ::free(name);
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

RegComp::RegComp(const char *_name, int _id, int _size) : id(_id), size(_size)
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
  addTemplate("test", { "velocity", "position" });
  addTemplate("test1", { "test", "position" });

  // TODO: Search by size
}

int EntityManager::getSystemId(const char *name)
{
  if (::strcmp(name, "update_position") == 0) return 0;
  if (::strcmp(name, "position_printer") == 0) return 1;
  if (::strcmp(name, "test_printer") == 0) return 2;
  return 0;
}

void EntityManager::addTemplate(const char *templ_name, std::initializer_list<const char*> compNames)
{
  templates.emplace_back();
  auto &templ = templates.back();
  templ.name = templ_name;
  templ.compMask.resize(reg_comp_count);
  templ.compMask.assign(reg_comp_count, false);

  for (const auto &name : compNames)
    templ.components.push_back({ 0, find_comp(name) });

  std::sort(templ.components.begin(), templ.components.end(),
    [](const Template::CompDesc &lhs, const Template::CompDesc &rhs) { return lhs.desc->id < rhs.desc->id; });

  int offset = 0;
  for (auto &c : templ.components)
  {
    c.offset = offset;
    offset += c.desc->size;

    templ.compMask[c.desc->id] = true;
  }
  templ.compMask[find_comp("eid")->id] = true;
  templ.compMask[find_comp("update_stage")->id] = true;
  templ.size = offset;

  for (size_t i = 0; i < storages.size(); ++i)
    if (storages[i].size == templ.size)
    {
      templ.storageId = i;
      break;
    }

  if (templ.storageId < 0)
  {
    storages.emplace_back();
    auto &s = storages.back();
    s.size = templ.size;
    templ.storageId = storages.size() - 1;
  }
}

EntityId EntityManager::createEntity(const char *templ_name)
{
  int templateId = -1;
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == templ_name)
    {
      templateId = i;
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
}

void EntityManager::tickImpl(int stage_id, const RawArg &stage)
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
      for (size_t compId = 0; compId < sys.desc->compMask.size(); ++compId)
        if (sys.desc->compMask[compId] && !templ.compMask[compId])
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
