#include "ecs.h"

#include "stages/dispatchEvent.stage.h"

#include <sstream>

REG_EVENT_INIT(EventOnEntityCreate);
REG_EVENT_INIT(EventOnEntityReady);
REG_EVENT_INIT(EventOnChangeDetected);

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

bool EntityTemplate::hasCompontent(int type_id, int name_id) const
{
  assert(name_id >= 0);
  for (const auto &c : components)
    if (c.desc->id == type_id && c.nameId == name_id)
      return true;
  return false;
}

// bool EntityTemplate::hasCompontent(int type_id, const char *name) const
// {
//   assert(name && name[0]);
//   for (const auto &c : components)
//     if (c.desc->id == type_id && c.name == name)
//       return true;
//   return false;
// }

int EntityTemplate::getCompontentOffset(int type_id, int name_id) const
{
  assert(name_id >= 0);
  for (const auto &c : components)
    if (c.desc->id == type_id && c.nameId == name_id)
      return c.id;
  return -1;
}

// int EntityTemplate::getCompontentOffset(int type_id, const char *name) const
// {
//   assert(name && name[0]);
//   for (const auto &c : components)
//     if (c.desc->id == type_id && c.name == name)
//       return c.id;
//   return -1;
// }

void EventStream::push(EntityId eid, uint8_t flags, int event_id, const RawArg &ev)
{
  ++count;

  int sz = sizeof(Header) + ev.size;
  int pos = 0;

  if (pushOffset + sz <= (int)data.size())
    pos = pushOffset;
  else
  {
    pos = pushOffset;
    data.resize(pushOffset + sz);
  }

  Header *header = (Header*)&data[pos];
  pos += sizeof(Header);

  header->eid = eid;
  header->flags = flags;
  header->eventId = event_id;
  header->eventSize = ev.size;

  ::memcpy(&data[pos], ev.mem, ev.size);

  pushOffset += sz;
}

eastl::tuple<EventStream::Header, RawArg> EventStream::pop()
{
  assert(count > 0);

  if (count <= 0)
    return {};

  EventStream::Header header = *(EventStream::Header*)&data[popOffset];
  popOffset += sizeof(EventStream::Header);

  uint8_t *mem = &data[popOffset];
  popOffset += header.eventSize;

  if (--count == 0)
  {
    popOffset = 0;
    pushOffset = 0;
  }

  return eastl::make_tuple(header, RawArg{ header.eventSize, mem });
}

void EntityManager::create()
{
  if (g_mgr)
    return;

  g_mgr = new EntityManager;
  g_mgr->init();
}

void EntityManager::release()
{
  delete g_mgr;
  g_mgr = nullptr;
}

EntityManager::EntityManager()
{
}

EntityManager::~EntityManager()
{
  for (auto &t : archetypes)
    t.clear();
  // for (Storage *s : storages)
  //   delete s;
  // TODO: Call dtors for components
}

void EntityManager::init()
{
  eidComp = find_comp("eid");
  eidCompId = eidComp->id;

  componentNames.emplace_back() = "eid";
  componentDescByNames.emplace_back() = eidComp;

  // storages.emplace_back() = componentDescByNames[0]->createStorage();
  // storages[0]->name = componentNames[0];
  // storages[0]->elemSize = eidComp->size;

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

      templatesDoc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(buffer);
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
    q.stageId = sys.desc->stageId > 0 ? sys.desc->stageId : sys.desc->eventId;
    q.sysId = sys.desc->id;
    q.desc = sys.desc->queryDesc;
  }

  for (const auto &sys : systems)
  {
    for (const auto &c : sys.desc->queryDesc.isTrueComponents)
      enableChangeDetection(c.name);
    for (const auto &c : sys.desc->queryDesc.isFalseComponents)
      enableChangeDetection(c.name);
  }
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

const char* EntityManager::getComponentName(int name_id) const
{
  if (name_id >= 0 && name_id < (int)componentNames.size())
    return componentNames[name_id].c_str();
  return nullptr;
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

static void add_component_to_template(const char *comp_type, int comp_name_id,
  EntityTemplate &templ,
  eastl::vector<eastl::string> &component_names,
  eastl::vector<const RegComp*> &component_desc_by_names/* ,
  eastl::vector<Storage*> &storages */)
{
  assert(comp_name_id >= 0 && comp_name_id < (int)component_names.size());

  auto res = eastl::find_if(templ.components.begin(), templ.components.end(), [&](const CompDesc &c) { return c.nameId == comp_name_id; });
  if (res != templ.components.end())
    return;

  templ.components.push_back({ 0, comp_name_id, hash_str(component_names[comp_name_id].c_str()), find_comp(comp_type)->size, find_comp(comp_type) });

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

  // if (component_names.size() > storages.size())
  // {
  //   const int size = storages.size();
  //   storages.resize(component_names.size());
  //   for (int i = size; i < (int)storages.size(); ++i)
  //     storages[i] = nullptr;
  // }

  // auto &storage = storages[nameId];
  // if (!storage)
  // {
  //   storage = desc->createStorage();
  //   assert(storage != nullptr);
  // }

  // if (storage->elemSize <= 0)
  // {
  //   storage->name = component_names[comp_name_id];
  //   storage->elemSize = desc->size;
  // }

  // assert(storage->elemSize == desc->size);
}

static void add_component_to_template(const char *comp_type, const char *comp_name,
  EntityTemplate &templ,
  eastl::vector<eastl::string> &component_names,
  eastl::vector<const RegComp*> &component_desc_by_names/* ,
  eastl::vector<Storage*> &storages */)
{
  add_component_to_template(comp_type, add_component_name(component_names, comp_name), templ, component_names, component_desc_by_names/* , storages */);
}

static void process_extends(EntityManager *mgr,
  EntityTemplate &templ,
  const eastl::vector<const char*> &extends,
  eastl::vector<eastl::string> &component_names,
  eastl::vector<const RegComp*> &component_desc_by_names/* ,
  eastl::vector<Storage*> &storages */)
{
  for (const auto &e : extends)
  {
    const int templateId = mgr->getTemplateId(e);
    templ.extends.push_back(templateId);
    for (const auto &c : mgr->templates[templateId].components)
      add_component_to_template(c.desc->name, c.nameId, templ, component_names, component_desc_by_names/* , storages */);
  }
}

void EntityManager::addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names, const eastl::vector<const char*> &extends)
{
  templates.emplace_back();
  auto &templ = templates.back();
  templ.docId = doc_id;
  templ.name = templ_name;
  templ.compMask.resize(reg_comp_count);
  templ.compMask.set(reg_comp_count, false);

  process_extends(this, templ, extends, componentNames, componentDescByNames/* , storages */);

  for (const auto &name : comp_names)
    add_component_to_template(name.first, name.second, templ, componentNames, componentDescByNames/* , storages */);

  eastl::sort(templ.components.begin(), templ.components.end(),
    [](const CompDesc &lhs, const CompDesc &rhs)
    {
      if (lhs.desc->id == rhs.desc->id)
        return lhs.nameId < rhs.nameId;
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

  // TODO: Create arhcetypes by componets list. Not per template
  Archetype &type = archetypes.emplace_back();
  type.storages.resize(templ.components.size());

  templ.archetypeId = archetypes.size() - 1;

  int compId = 0;
  for (auto &storage : type.storages)
  {
    const auto &c = templ.components[compId++];

    storage.storage = componentDescByNames[c.nameId]->createStorage();
    assert(storage != nullptr);

    storage.name = hash_str(componentNames[c.nameId].c_str());
    storage->name = componentNames[c.nameId];
    storage->elemSize = componentDescByNames[c.nameId]->size;

    // type.storageMap[hash_str(componentNames[c.nameId].c_str())] = storage;
  }

  type.storages.emplace_back().storage = eidComp->createStorage();
  type.storages.back().name = hash::cstr("eid");
  type.storages.back()->name = eidComp->name;
  type.storages.back()->elemSize = eidComp->size;
  // type.storageMap[hash::cstr("eid")] = type.storages.back();
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

void EntityManager::createEntity(const char *templ_name, const JFrameValue &comps)
{
  JDocument doc;
  doc.CopyFrom(comps, doc.GetAllocator());

  CreateQueueData q;
  q.templanemName = templ_name;
  q.components = eastl::move(doc);
  createQueue.emplace_back(eastl::move(q));
}

void EntityManager::deleteEntity(const EntityId &eid)
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

template <typename Allocator>
static void override_component(JFrameValue &dst, JFrameValue &src, const char *name, Allocator &allocator)
{
  if (!dst.HasMember(name))
    dst.AddMember(rapidjson::StringRef(name), src, allocator);
}

template <typename Allocator>
static void process_templates(EntityManager *mgr,
  const EntityTemplate &templ,
  const JDocument &templates_doc,
  JFrameValue &dst,
  Allocator &allocator)
{
  for (int id : templ.extends)
  {
    const JValue &extendsValue = templates_doc["$templates"][mgr->templates[id].docId];
    for (auto it = extendsValue["$components"].MemberBegin(); it != extendsValue["$components"].MemberEnd(); ++it)
    {
      JFrameValue v(rapidjson::kObjectType);
      v.CopyFrom(it->value, allocator);
      override_component(dst, v, it->name.GetString(), allocator);
    }

    process_templates(mgr, mgr->templates[id], templates_doc, dst, allocator);
  }
}

EntityId EntityManager::createEntitySync(const char *templ_name, const JValue &comps)
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

  JFrameAllocator tmp_allocator;
  JFrameValue tmpValue(rapidjson::kObjectType);
  tmpValue.CopyFrom(value["$components"], tmp_allocator);

  process_templates(this, templ, templatesDoc, tmpValue, tmp_allocator);

  if (!comps.IsNull())
    for (auto compIter = comps.MemberBegin(); compIter != comps.MemberEnd(); ++compIter)
      tmpValue[compIter->name.GetString()]["$value"].CopyFrom(compIter->value, tmp_allocator);

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
  e.archetypeId = templ.archetypeId;
  e.ready = true;

  archetypes[e.archetypeId].entitiesCount++;

  // entityDescs.resize(entities.size());

  // auto &entityDesc = entityDescs[e.eid.index];

  // for (const auto &c : templ.components)
  // {
  //   auto &storage = storages[c.nameId];

  //   uint8_t *mem = nullptr;
  //   int offset = 0;
  //   eastl::tie(mem, offset) = storage->allocate();

  //   c.desc->init(mem, tmpValue[getComponentName(c.nameId)]);
  //   e.componentOffsets.push_back(offset);

  //   entityDesc.offsetsByNameId[c.nameId] = offset;
  // }

  // // eid
  // {
  //   uint8_t *mem = nullptr;
  //   int offset = 0;
  //   eastl::tie(mem, offset) = storages[0]->allocate();

  //   JFrameAllocator allocator;

  //   JFrameValue val(rapidjson::kObjectType);
  //   JFrameValue eidVal(rapidjson::kNumberType);
  //   eidVal.SetInt(e.eid.handle);
  //   val.AddMember("$value", eastl::move(eidVal), allocator);

  //   eidComp->init(mem, val);
  //   e.componentOffsets.push_back(offset);

  //   entityDesc.offsetsByNameId[0] = offset;
  // }

  {
    int compId = 0;
    auto &type = archetypes[templ.archetypeId];
    for (const auto &c : templ.components)
    {
      uint8_t *mem = nullptr;
      int offset = 0;
      eastl::tie(mem, offset) = type.storages[compId++]->allocate();

      c.desc->init(mem, tmpValue[getComponentName(c.nameId)]);
    }

    // eid
    {
      uint8_t *mem = nullptr;
      int offset = 0;
      eastl::tie(mem, offset) = type.storages.back()->allocate();
      new (mem) EntityId(e.eid);

      e.indexInArchetype = offset / sizeof(EntityId);
    }
  }

  sendEventSync(e.eid, EventOnEntityCreate{});

  return e.eid;
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

      // entityDescs[eid.index].offsetsByNameId.clear();

      const auto &templ = templates[entity.templateId];

      // assert(templ.components.size() == entity.componentOffsets.size() - 1);

      // int compIndex = 0;
      // for (const auto &c : templ.components)
      // {
      //   auto &storage = storages[c.nameId];
      //   const int offset = entity.componentOffsets[compIndex++];
      //   storage->deallocate(offset);
      // }

      // // eid
      // storages[0]->deallocate(entity.componentOffsets[compIndex]);

      {
        int compId = 0;
        auto &type = archetypes[templ.archetypeId];
        for (const auto &c : templ.components)
        {
          auto &storage = type.storages[compId++];
          const int offset = entity.indexInArchetype * storage->elemSize;
          storage->deallocate(offset);
        }

        // eid
        type.storages.back()->deallocate(entity.indexInArchetype * type.storages.back()->elemSize);

        type.entitiesCount--;
      }

      entity.eid.generation = (entity.eid.generation + 1) % std::numeric_limits<uint16_t>::max();
      entity.ready = false;
      entity.indexInArchetype = -1;
      // entity.componentOffsets.clear();

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

  // if (shouldInvalidateQueries)
  //   for (auto &s : storages)
  //     s->invalidate();
  if (shouldInvalidateQueries)
    for (auto &t : archetypes)
      t.invalidate();

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

  bool queriesInvalidated = false;
  if (shouldInvalidateQueries)
  {
    queriesInvalidated = true;
    for (auto &q : queries)
      invalidateQuery(q);
  }
  else
  {
    for (auto &q : queries)
      if (q.dirty)
      {
        queriesInvalidated = true;
        invalidateQuery(q);
      }
  }

  if (queriesInvalidated)
    sendEventBroadcastSync(EventOnChangeDetected{});

  const int streamIndex = currentEventStream;
  currentEventStream = (currentEventStream + 1) % events.size();
  assert(currentEventStream != streamIndex);

  {
    FrameSnapshot snapshot;
    fillFrameSnapshot(snapshot);

    while (events[streamIndex].count)
    {
      EventStream::Header header;
      RawArg ev;
      eastl::tie(header, ev) = events[streamIndex].pop();

      if (header.flags & EventStream::kBroadcast)
        sendEventBroadcastSync(header.eventId, ev);
      else
        sendEventSync(header.eid, header.eventId, ev);

      tick(DispatchEventStage{ header.eid, header.eventId, ev });
    }

    checkFrameSnapshot(snapshot);
  }
}

bool Archetype::hasCompontent(const HashedString &name) const
{
  return getComponentIndex(name) >= 0;
}

bool Archetype::hasCompontent(const ConstHashedString &name) const
{
  return getComponentIndex(name) >= 0;
}

int Archetype::getComponentIndex(const HashedString &name) const
{
  for (int i = 0; i < (int)storages.size(); ++i)
    if (storages[i].name == name)
      return i;
  return -1;
}

int Archetype::getComponentIndex(const ConstHashedString &name) const
{
  return getComponentIndex(HashedString(name));
}

void EntityManager::invalidateQuery(Query &query)
{
  assert(query.desc.isValid());

  query.dirty = false;
  query.chunksCount = 0;
  query.entitiesCount = 0;
  query.chunks.clear();
  query.entitiesInChunk.clear();

  for (auto &type : archetypes)
  {
    bool ok = true;
    for (const auto &c : query.desc.components)
      if (!type.hasCompontent(c.name))
      {
        ok = false;
        break;
      }

    if (ok)
    {
      for (const auto &c : query.desc.haveComponents)
        if (!type.hasCompontent(c.name))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      for (const auto &c : query.desc.notHaveComponents)
        if (type.hasCompontent(c.name))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      // TODO: Assert on type mismatch
      for (const auto &c : query.desc.isTrueComponents)
        if (!type.hasCompontent(c.name))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      // TODO: Assert on type mismatch
      for (const auto &c : query.desc.isFalseComponents)
        if (!type.hasCompontent(c.name))
        {
          ok = false;
          break;
        }
    }

    if (ok)
    {
      int begin = -1;
      for (int i = 0; i < type.entitiesCount; ++i)
      {
        ok = true;

        for (const auto &c : query.desc.isTrueComponents)
        {
          const int compIdx = type.getComponentIndex(c.name);
          auto &storage = type.storages[compIdx];
          if (!storage->getByIndex<bool>(i))
          {
            ok = false;
            break;
          }
        }

        if (ok)
          for (const auto &c : query.desc.isFalseComponents)
          {
            const int compIdx = type.getComponentIndex(c.name);
            auto &storage = type.storages[compIdx];
            if (storage->getByIndex<bool>(i))
            {
              ok = false;
              break;
            }
          }

        if (begin < 0)
          begin = i;

        if (!ok || i == type.entitiesCount - 1)
        {
          const int componentsCount = query.desc.components.size();

          ++query.chunksCount;
          query.entitiesCount += i - begin + 1;
          query.chunks.resize(query.chunks.size() + componentsCount);
          query.entitiesInChunk.resize(query.chunksCount);
          query.entitiesInChunk[query.chunksCount - 1] = i - begin + 1;

          int compIdx = 0;
          for (const auto &c : query.desc.components)
          {
            auto &storage = type.storages[type.getComponentIndex(c.name)];

            QueryChunk chunk;
            chunk.beginData = storage->getRawByIndex(begin);
            chunk.endData = storage->getRawByIndex(i + 1);

            query.chunks[compIdx + (query.chunksCount - 1) * componentsCount] = chunk;
            compIdx++;
          }

          begin = -1;
        }
      }
    }
  }
}

// void EntityManager::invalidateQuery(Query &query)
// {
//   assert(query.desc.isValid());

//   query.dirty = false;
//   query.eids.clear();

//   for (auto &e : entities)
//   {
//     if (!e.ready)
//       continue;

//     const auto &templ = templates[e.templateId];

//     bool ok = true;
//     for (const auto &c : query.desc.components)
//       if (c.desc->id != g_mgr->eidCompId && c.desc->id != query.stageId && !templ.hasCompontent(c.desc->id, c.nameId))
//       {
//         ok = false;
//         break;
//       }

//     if (ok)
//     {
//       for (const auto &c : query.desc.haveComponents)
//         if (!templ.hasCompontent(c.desc->id, c.nameId))
//         {
//           ok = false;
//           break;
//         }
//     }

//     if (ok)
//     {
//       for (const auto &c : query.desc.notHaveComponents)
//         if (templ.hasCompontent(c.desc->id, c.nameId))
//         {
//           ok = false;
//           break;
//         }
//     }

//     if (ok)
//     {
//       for (const auto &c : query.desc.isTrueComponents)
//       {
//         const auto &entity = entities[eid2idx(e.eid)];
//         if (!templ.hasCompontent(c.desc->id, c.nameId))
//         {
//           ok = false;
//           break;
//         }
//         if (!archetypes[templ.archetypeId].storageMap[c.name]->getByIndex<bool>(entity.indexInArchetype))
//         {
//           ok = false;
//           break;
//         }
//       }
//     }

//     if (ok)
//     {
//       for (const auto &c : query.desc.isFalseComponents)
//       {
//         const auto &entity = entities[eid2idx(e.eid)];
//         if (!templ.hasCompontent(c.desc->id, c.nameId))
//         {
//           ok = false;
//           break;
//         }
//         if (archetypes[templ.archetypeId].storageMap[c.name]->getByIndex<bool>(entity.indexInArchetype))
//         {
//           ok = false;
//           break;
//         }
//       }
//     }

//     if (ok)
//       query.eids.push_back(e.eid);
//   }
// }

void EntityManager::fillFrameSnapshot(FrameSnapshot &snapshot) const
{
  // TODO: Optimize
  for (const auto &type : archetypes)
  {
    for (const auto &name : trackComponents)
    {
      int index = type.getComponentIndex(name);
      if (index >= 0)
      {
        const size_t sz = type.storages[index].storage->size();
        snapshot.emplace_back() = alloc_frame_mem(sz);
        ::memcpy(snapshot.back(), type.storages[index].storage->data(), sz);
      }
    }
  }
}

void EntityManager::checkFrameSnapshot(const FrameSnapshot &snapshot)
{
  // TODO: Optimize
  int i = 0;
  for (const auto &type : archetypes)
  {
    for (const auto &name : trackComponents)
    {
      int index = type.getComponentIndex(name);
      if (index >= 0)
      {
        if (::memcmp(snapshot[i++], type.storages[index].storage->data(), type.storages[index].storage->size()))
        {
          for (auto &q : queries)
            q.dirty = true;
          // break;
          // TODO: Correct way to interrupt the cycle
          return;
        }
      }
    }
  }

  // int i = 0;
  // for (int nameId : trackComponents)
  //   if (::memcmp(snapshot[i++], storages[nameId]->data(), storages[nameId]->size()))
  //   {
  //     for (auto &q : queries)
  //       q.dirty = true;
  //     break;
  //   }
}

void EntityManager::tickStage(int stage_id, const RawArg &stage)
{
  FrameSnapshot snapshot;
  fillFrameSnapshot(snapshot);

  for (const auto &sys : systems)
    if (sys.desc->stageId == stage_id)
    {
      auto &query = queries[sys.desc->id];
      (sys.desc->*sys.desc->stageFn)(query, stage/* , storages.data() */);
    }

  checkFrameSnapshot(snapshot);
}

void EntityManager::sendEvent(EntityId eid, int event_id, const RawArg &ev)
{
  events[currentEventStream].push(eid, EventStream::kTarget, event_id, ev);
}

void EntityManager::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
{
  auto &e = entities[eid.index];
  auto &type = archetypes[e.archetypeId];

  for (const auto &sys : systems)
    if (sys.desc->eventId == event_id)
    {
      // TODO: Add checks for isTrue, isFalse, have, notHave
      bool ok = true;
      for (const auto &c : sys.desc->queryDesc.components)
        if (!type.hasCompontent(c.name))
        {
          ok = false;
          break;
        }

      if (ok)
      {
        Query query;
        query.desc = sys.desc->queryDesc;
        query.chunksCount = 1;

        const int componentsCount = query.desc.components.size();
        query.chunks.resize(componentsCount);

        query.entitiesInChunk.resize(query.chunksCount);
        query.entitiesInChunk[query.chunksCount - 1] = 1;

        int compIdx = 0;
        for (const auto &c : query.desc.components)
        {
          auto &storage = type.storages[type.getComponentIndex(c.name)];

          QueryChunk chunk;
          chunk.beginData = storage->getRawByIndex(e.indexInArchetype);
          chunk.endData = storage->getRawByIndex(e.indexInArchetype + 1);

          query.chunks[compIdx + (query.chunksCount - 1) * componentsCount] = chunk;
          compIdx++;
        }

        (sys.desc->*sys.desc->stageFn)(query, ev/* , &storages[0] */);
      }
    }
}

void EntityManager::sendEventBroadcast(int event_id, const RawArg &ev)
{
  events[currentEventStream].push(EntityId{}, EventStream::kBroadcast, event_id, ev);
}

void EntityManager::sendEventBroadcastSync(int event_id, const RawArg &ev)
{
  for (const auto &sys : systems)
    if (sys.desc->eventId == event_id)
    {
      auto &query = queries[sys.desc->id];
      (sys.desc->*sys.desc->stageFn)(query, ev/* , storages.data() */);
    }
}

void EntityManager::enableChangeDetection(const HashedString &name)
{
  if (name == hash::cstr("eid"))
    return;
  std::cout << "enableChangeDetection: " << name.str << std::endl;
  trackComponents.insert(name);
}

void EntityManager::disableChangeDetection(const HashedString &name)
{
  std::cout << "disableChangeDetection: " << name.str << std::endl;
  trackComponents.erase(name);
}