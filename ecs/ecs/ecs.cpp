#include "ecs.h"

#include <sstream>

REG_EVENT_INIT(EventOnEntityCreate);
REG_EVENT_INIT(EventOnEntityReady);

EntityManager *g_mgr = nullptr;

RegSys *reg_sys_head = nullptr;
int reg_sys_count = 0;

RegComp *reg_comp_head = nullptr;
int reg_comp_count = 0;

RegSys::RegSys(const char *_name, int _id, bool need_order) : id(_id), needOrder(need_order)
{
  if (_name)
    name = ::_strdup(_name);
}

RegSys::~RegSys()
{
  if (name)
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

int Template::getCompontentOffset(int id, const char *name) const
{
  if (!name || !name[0])
    return -1;
  for (const auto &c : components)
    if (c.desc->id == id && c.name == name)
      return c.id;
  return -1;
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

eastl::tuple<EntityId, int, RawArg> EventStream::pop()
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

  return eastl::make_tuple(eid, event_id, RawArg{ event_size, mem });
}

void EntityManager::init()
{
  if (g_mgr)
    return;

  g_mgr = new EntityManager;
}

void EntityManager::release()
{
  delete g_mgr;
  g_mgr = nullptr;
}

EntityManager::EntityManager()
{
  {
    FILE *file = nullptr;
    ::fopen_s(&file, "templates.json", "rb");
    if (file)
    {
      size_t sz = ::ftell(file);
      ::fseek(file, 0, SEEK_END);
      sz = ::ftell(file) - sz;
      ::fseek(file, 0, SEEK_SET);

      char *buffer = new char[sz + 1];
      buffer[sz] = '\0';
      ::fread(buffer, 1, sz, file);
      ::fclose(file);

      templatesDoc.Parse(buffer);
      delete [] buffer;

      assert(templatesDoc.HasMember("$order"));
      assert(templatesDoc["$order"].IsArray());
      for (int i = 0; i < (int)templatesDoc["$order"].Size(); ++i)
        order.emplace_back(templatesDoc["$order"][i].GetString());

      assert(templatesDoc.HasMember("$templates"));
      assert(templatesDoc["$templates"].IsArray());
      for (int i = 0; i < (int)templatesDoc["$templates"].Size(); ++i)
      {
        const JValue &templ = templatesDoc["$templates"][i];

        eastl::vector<eastl::pair<const char*, const char*>> comps;
        eastl::vector<const char*> extends;
        if (templ.HasMember("$extends"))
          for (int j = 0; j < (int)templ["$extends"].Size(); ++j)
            extends.push_back(templ["$extends"][j].GetString());

        const JValue &templComps = templ["$components"];
        for (auto compIter = templComps.MemberBegin(); compIter != templComps.MemberEnd(); ++compIter)
        {
          const JValue &type = compIter->value["$type"];
          comps.push_back({ type.GetString(), compIter->name.GetString() });

          if (compIter->value.HasMember("$array"))
          {
            // Debug check
            std::ostringstream oss;
            oss << "[" << compIter->value["$array"].Size() << "]";
            std::string res = oss.str();
            int offset = type.GetStringLength() - (int)res.length();
            assert(offset > 0);
            const char *tail = type.GetString() + offset;
            assert(::strcmp(res.c_str(), tail) == 0);
          }
        }

        addTemplate(i, templ["$name"].GetString(), comps, extends);
      }
    }
  }

  for (const RegSys *sys = reg_sys_head; sys; sys = sys->next)
    const_cast<RegSys*>(sys)->init(this);

  for (const RegSys *sys = reg_sys_head; sys; sys = sys->next)
    systems.push_back({ sys->needOrder ? getSystemWeight(sys->name) : -1, sys });

  eastl::sort(systems.begin(), systems.end(),
    [](const System &lhs, const System &rhs) { return lhs.weight < rhs.weight; });

  queries.resize(reg_sys_count);
  for (const auto &sys : systems)
  {
    auto &q = queries[sys.desc->id];
    q.stageId = sys.desc->stageId;
    q.sys = sys.desc;
  }

  for (const auto &sys : systems)
  {
    for (const auto &c : sys.desc->isTrueComponents)
      trackComponents.insert(c.nameId);
    for (const auto &c : sys.desc->isFalseComponents)
      trackComponents.insert(c.nameId);
  }

  for (const auto &sys : systems)
  {
    for (int nameId : trackComponents)
    {
      auto it = eastl::find_if(sys.desc->rwComponents.begin(), sys.desc->rwComponents.end(), [&](int id)
      {
        return sys.desc->components[id].nameId == nameId;
      });

      if (it != sys.desc->rwComponents.end())
      {

      }
    }
  }

  eidCompId = find_comp("eid")->id;
}

EntityManager::~EntityManager()
{
  for (Storage *s : storages)
    delete s;
  // TODO: Call dtors for components
}

int EntityManager::getSystemWeight(const char *name) const
{
  auto res = eastl::find_if(order.begin(), order.end(), [name](const eastl::string &n) { return n == name; });
  assert(res != order.end());
  return (int)(res - order.begin());
}

static int add_component_name(eastl::vector<eastl::string> &to, const char *name)
{
  auto it = eastl::find(to.begin(), to.end(), name);
  if (it == to.end())
  {
    to.emplace_back(name);
    return to.size() - 1;
  }
  return it - to.begin();
}

int EntityManager::getComponentNameId(const char *name) const
{
  auto it = eastl::find(componentNames.begin(), componentNames.end(), name);
  return it == componentNames.end() ? -1 : it - componentNames.begin();
}

const RegComp* EntityManager::getComponentDescByName(const char *name) const
{
  int nameId = getComponentNameId(name);
  assert(nameId >= 0);
  return componentDescByNames[nameId];
}

static void add_component_to_template(const char *comp_type, const char *comp_name,
  Template &templ,
  eastl::vector<eastl::string> &component_names,
  eastl::vector<const RegComp*> &component_desc_by_names,
  eastl::vector<Storage*> &storages)
{
  templ.components.push_back({ 0, add_component_name(component_names, comp_name), comp_name, find_comp(comp_type) });

  if (component_desc_by_names.size() < component_names.size())
  {
    const int size = component_names.size();
    component_desc_by_names.resize(component_names.size());
    for (int i = size; i < (int)component_names.size(); ++i)
      component_desc_by_names[i] = nullptr;
  }

  const int nameId = templ.components.back().nameId;
  const RegComp *desc = templ.components.back().desc;
  assert(desc != nullptr);
  assert(component_desc_by_names[nameId] == nullptr || component_desc_by_names[nameId] == desc);
  component_desc_by_names[nameId] = desc;

  if (component_names.size() > storages.size())
  {
    const int size = storages.size();
    storages.resize(component_names.size());
    for (int i = size; i < (int)storages.size(); ++i)
      storages[i] = nullptr;
  }

  auto &storage = storages[nameId];
  if (!storage)
  {
    storage = desc->createStorage();
    assert(storage != nullptr);
  }

  if (storage->elemSize <= 0)
  {
    storage->name = comp_name;
    storage->elemSize = desc->size;
  }

  assert(storage->elemSize == desc->size);
}

void EntityManager::addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names, const eastl::vector<const char*> &extends)
{
  templates.emplace_back();
  auto &templ = templates.back();
  templ.docId = doc_id;
  templ.name = templ_name;
  templ.compMask.resize(reg_comp_count);
  templ.compMask.set(reg_comp_count, false);

  for (const auto &e : extends)
  {
    const int templateId = getTemplateId(e);
    templ.extends.push_back(templateId);
    for (const auto &c : templates[templateId].components)
      add_component_to_template(c.desc->name, c.name.c_str(), templ, componentNames, componentDescByNames, storages);
  }

  for (const auto &name : comp_names)
    add_component_to_template(name.first, name.second, templ, componentNames, componentDescByNames, storages);

  eastl::sort(templ.components.begin(), templ.components.end(),
    [](const CompDesc &lhs, const CompDesc &rhs)
    {
      if (lhs.desc->id == rhs.desc->id)
        return lhs.name < rhs.name;
      return lhs.desc->id < rhs.desc->id;
    });

  int offset = 0;
  for (size_t i = 0; i < templ.components.size(); ++i)
  {
    auto &c = templ.components[i];
    c.id = i;
    offset += c.desc->size;

    templ.compMask[c.desc->id] = true;
  }
  templ.size = offset;

  templ.remaps.resize(reg_sys_count);
  for (const RegSys *sys = reg_sys_head; sys; sys = sys->next)
    sys->initRemap(templ.components, templ.remaps[sys->id]);
}

void EntityManager::createEntity(const char *templ_name, const JValue &comps)
{
  JDocument doc;
  doc.CopyFrom(comps, doc.GetAllocator());

  CreateQueueData q;
  q.templanemName = templ_name;
  q.components = eastl::move(doc);
  createQueue.emplace_back(eastl::move(q));
}

void EntityManager::deleteEntity(EntityId eid)
{
  deleteQueue.emplace_back(eid);
}

void EntityManager::waitFor(EntityId eid, std::future<bool> && value)
{
  assert(std::find_if(asyncValues.begin(), asyncValues.end(), [eid](const AsyncValue &v) { return v.eid == eid; }) == asyncValues.end());
  asyncValues.emplace_back(eid, std::move(value));
  entities[eid2idx(eid)].ready = false;
}

int EntityManager::getTemplateId(const char *name)
{
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == name)
      return i;
  assert(false);
  return -1;
}

void EntityManager::createEntitySync(const char *templ_name, const JValue &comps)
{
  int templateId = -1;
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == templ_name)
    {
      templateId = (int)i;
      break;
    }
  assert(templateId >= 0);

  auto &templ = templates[templateId];

  const JValue &value = templatesDoc["$templates"][templ.docId];

  JDocument tmpDoc;
  JValue tmpValue(rapidjson::kObjectType);
  tmpValue.CopyFrom(value["$components"], tmpDoc.GetAllocator());
  for (int id : templ.extends)
  {
    const JValue &extendsValue = templatesDoc["$templates"][templates[id].docId];
    for (auto it = extendsValue["$components"].MemberBegin(); it != extendsValue["$components"].MemberEnd(); ++it)
    {
      JValue v(rapidjson::kObjectType);
      v.CopyFrom(it->value, tmpDoc.GetAllocator());
      tmpValue.AddMember(rapidjson::StringRef(it->name.GetString()), v, tmpDoc.GetAllocator());
    }
  }

  if (!comps.IsNull())
    for (auto compIter = comps.MemberBegin(); compIter != comps.MemberEnd(); ++compIter)
      tmpValue[compIter->name.GetString()]["$value"].CopyFrom(compIter->value, tmpDoc.GetAllocator());

  int freeIndex = -1;
  if (!freeEntityQueue.empty())
  {
    freeIndex = freeEntityQueue.front();
    freeEntityQueue.pop();
  }

  auto &e = freeIndex >= 0 ? entities[freeIndex] : entities.emplace_back();
  if (freeIndex < 0)
  {
    e.eid.generation = std::rand() % eastl::numeric_limits<uint16_t>::max();
    e.eid.index = entities.size() - 1;
  }
  e.templateId = templateId;
  e.ready = true;

  for (const auto &c : templ.components)
  {
    auto &storage = storages[c.nameId];

    uint8_t *mem = nullptr;
    int offset = 0;
    eastl::tie(mem, offset) = storage->allocate();

    c.desc->init(mem, tmpValue[c.name.c_str()]);
    e.componentOffsets.push_back(offset);
  }

  sendEventSync(e.eid, EventOnEntityCreate{});
}

void EntityManager::tick()
{
  bool shouldInvalidateQueries = false;

  while (!deleteQueue.empty())
  {
    EntityId eid = deleteQueue.front();

    auto &entity = entities[eid2idx(eid)];
    if (eid.generation == entity.eid.generation)
    {
      shouldInvalidateQueries = true;

      const auto &templ = templates[entity.templateId];

      assert(templ.components.size() == entity.componentOffsets.size());

      int compIndex = 0;
      for (const auto &c : templ.components)
      {
        auto &storage = storages[c.nameId];
        const int offset = entity.componentOffsets[compIndex++];
        storage->deallocate(offset);
      }

      entity.eid.generation = (entity.eid.generation + 1) % std::numeric_limits<uint16_t>::max();
      entity.ready = false;
      entity.componentOffsets.clear();

      freeEntityQueue.push(eid.index);
    }

    deleteQueue.pop();
  }

  while (!createQueue.empty())
  {
    shouldInvalidateQueries = true;

    const auto &q = createQueue.front();
    createEntitySync(q.templanemName.c_str(), q.components);
    createQueue.pop();
  }

  if (shouldInvalidateQueries)
    for (auto &s : storages)
      s->invalidate();

  for (const auto &v : asyncValues)
  {
    const bool ready = v.isReady();
    if (ready)
    {
      shouldInvalidateQueries = true;
      entities[eid2idx(v.eid)].ready = ready;
      sendEventSync(v.eid, EventOnEntityReady{});
    }
  }

  if (!asyncValues.empty())
    asyncValues.erase(eastl::remove_if(asyncValues.begin(), asyncValues.end(), [](const AsyncValue &v) { return v.isReady(); }), asyncValues.end());

  if (shouldInvalidateQueries)
    for (auto &q : queries)
      invalidateQuery(q);
  else
  {
    for (auto &q : queries)
      if (q.dirty)
        invalidateQuery(q);
  }

  const int streamIndex = currentEventStream;
  currentEventStream = (currentEventStream + 1) % events.size();
  assert(currentEventStream != streamIndex);

  while (events[streamIndex].count)
  {
    EntityId eid;
    int event_id = -1;
    RawArg ev;
    eastl::tie(eid, event_id, ev) = events[streamIndex].pop();
    sendEventSync(eid, event_id, ev);
  }
}

void EntityManager::invalidateQuery(Query &query)
{
  query.dirty = false;

  if (query.stageId < 0 && query.sys->eventId >= 0)
    return;

  query.eids.clear();

#ifdef ECS_PACK_QUERY_DATA
  query.data.clear();
#endif

  for (auto &e : entities)
  {
    if (!e.ready)
      continue;

    const auto &templ = templates[e.templateId];

    bool ok = true;
    for (const auto &c : query.sys->components)
      if (c.desc->id != g_mgr->eidCompId && c.desc->id != query.stageId && !templ.hasCompontent(c.desc->id, c.name.c_str()))
      {
        ok = false;
        break;
      }

    if (ok)
    {
      for (const auto &c : query.sys->haveComponents)
        if (!templ.hasCompontent(c.desc->id, c.name.c_str()))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      for (const auto &c : query.sys->notHaveComponents)
        if (templ.hasCompontent(c.desc->id, c.name.c_str()))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      for (const auto &c : query.sys->isTrueComponents)
      {
        const auto &entity = entities[eid2idx(e.eid)];
        const int offset = entity.componentOffsets[templ.getCompontentOffset(c.desc->id, c.name.c_str())];

        if (!templ.hasCompontent(c.desc->id, c.name.c_str()) || !storages[c.nameId]->get<bool>(offset))
        {
          ok = false;
          break;
        }
      }
    }

    if (ok)
    {
      for (const auto &c : query.sys->isFalseComponents)
      {
        const auto &entity = entities[eid2idx(e.eid)];
        const int offset = entity.componentOffsets[templ.getCompontentOffset(c.desc->id, c.name.c_str())];

        if (!templ.hasCompontent(c.desc->id, c.name.c_str()) || storages[c.nameId]->get<bool>(offset))
        {
          ok = false;
          break;
        }
      }
    }

    if (ok)
    {
      query.eids.push_back(e.eid);

#ifdef ECS_PACK_QUERY_DATA
      size_t sz = query.data.size();
      size_t remapSz = templ.remaps[query.sys->id].size();
      query.data.resize(sz + 1 + remapSz + e.componentOffsets.size());
      eastl::copy(templ.remaps[query.sys->id].begin(), templ.remaps[query.sys->id].end(), query.data.begin() + sz);
      query.data[sz + remapSz] = e.componentOffsets.size();
      eastl::copy(e.componentOffsets.begin(), e.componentOffsets.end(), query.data.begin() + sz + 1 + remapSz);
#endif
    }
  }
}

void EntityManager::tickStage(int stage_id, const RawArg &stage)
{
  // TODO: Fast stack allocator
  eastl::vector<Storage*> tmpCopy;
  for (int nameId : trackComponents)
  {
    tmpCopy.emplace_back() = componentDescByNames[nameId]->createStorage();
    storages[nameId]->copyTo(tmpCopy.back());
  }

  for (const auto &sys : systems)
    if (sys.desc->stageId == stage_id)
    {
      auto &query = queries[sys.desc->id];
      (sys.desc->*sys.desc->stageFn)(query.eids.size(), query.eids.data(), stage, storages.data());
    }

  bool changed = false;

  int i = 0;
  for (int nameId : trackComponents)
    if (::memcmp(tmpCopy[i++]->data(), storages[nameId]->data(), storages[nameId]->size()))
    {
      changed = true;
      break;
    }

  for (Storage *s : tmpCopy)
    delete s;

  // Invalidate only "is-true" "is-false" queries
  if (changed)
    for (auto &q : queries)
      q.dirty = true;
}

void EntityManager::sendEvent(EntityId eid, int event_id, const RawArg &ev)
{
  events[currentEventStream].push(eid, event_id, ev);
}

void EntityManager::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
{
  auto &e = entities[eid2idx(eid)];

  const auto &templ = templates[e.templateId];

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
        (sys.desc->*sys.desc->stageFn)(1, &eid, ev, &storages[0]);
    }
}

