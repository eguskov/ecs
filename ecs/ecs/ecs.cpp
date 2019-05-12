#include "ecs.h"

#include "stages/dispatchEvent.stage.h"

#include <sstream>

REG_EVENT_INIT(EventOnEntityCreate);
REG_EVENT_INIT(EventOnEntityReady);
REG_EVENT_INIT(EventOnChangeDetected);

EntityManager *g_mgr = nullptr;

SystemDescription *reg_sys_head = nullptr;
int reg_sys_count = 0;

ComponentDescription *reg_comp_head = nullptr;
int reg_comp_count = 0;

PersistentQueryDescription *reg_query_head = nullptr;
int reg_query_count = 0;

IndexDescription *reg_index_head = nullptr;
int reg_index_count = 0;

inline static bool has_components(const Archetype &type, const eastl::vector<Component> &components)
{
  for (const auto &c : components)
    if (!type.hasCompontent(c.name))
      return false;
  return true;
}

inline static bool not_have_components(const Archetype &type, const eastl::vector<Component> &components)
{
  for (const auto &c : components)
    if (type.hasCompontent(c.name))
      return false;
  return true;
}

SystemDescription::~SystemDescription()
{
  if (stageName)
    ::free(stageName);
}

const SystemDescription *find_system(const ConstHashedString &name)
{
  const SystemDescription *head = reg_sys_head;
  while (head)
  {
    if (head->name == name)
      return head;
    head = head->next;
  }
  return nullptr;
}

ComponentDescription::ComponentDescription(const char *_name, int _size) : id(reg_comp_count), size(_size)
{
  name = ::_strdup(_name);
  next = reg_comp_head;
  reg_comp_head = this;
  ++reg_comp_count;
}

ComponentDescription::~ComponentDescription()
{
  ::free(name);
}

const ComponentDescription *find_component(const char *name)
{
  const ComponentDescription *head = reg_comp_head;
  while (head)
  {
    if (::strcmp(head->name, name) == 0)
      return head;
    head = head->next;
  }
  return nullptr;
}

PersistentQueryDescription::PersistentQueryDescription(const ConstHashedString &_name, const ConstQueryDescription &_desc, filter_t &&f) : name(_name), desc(_desc), filter(eastl::move(f))
{
  next = reg_query_head;
  reg_query_head = this;
  ++reg_query_count;
}

IndexDescription::IndexDescription(const ConstHashedString &_name, const ConstHashedString &component_name, const ConstQueryDescription &_desc, filter_t &&f) : name(_name), componentName(component_name), desc(_desc), filter(eastl::move(f))
{
  next = reg_index_head;
  reg_index_head = this;
  ++reg_index_count;
}

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
  ASSERT(count > 0);

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

  jobmanager::release();
}

EntityManager::EntityManager()
{
}

EntityManager::~EntityManager()
{
  for (auto &t : archetypes)
    t.clear();
}

void EntityManager::init()
{
  jobmanager::init();

  eidComp = find_component("eid");
  eidCompId = eidComp->id;

  componentDescByNames[hash::cstr("eid")] = eidComp;

  {
    FILE *file = nullptr;
    ::fopen_s(&file, "data/templates.json", "rb");
    ASSERT_FMT(file != nullptr, "data/templates.json not found!");
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

      ASSERT(templatesDoc.HasMember("$order"));
      ASSERT(templatesDoc["$order"].IsArray());
      for (int i = 0; i < (int)templatesDoc["$order"].Size(); ++i)
        order.emplace_back(templatesDoc["$order"][i].GetString());

      ASSERT(templatesDoc.HasMember("$templates"));
      ASSERT(templatesDoc["$templates"].IsArray());
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

          #ifdef _DEBUG
          if (compIter->value.HasMember("$array"))
          {
            // Debug check
            std::ostringstream oss;
            oss << "[" << compIter->value["$array"].Size() << "]";
            std::string res = oss.str();
            int offset = type.GetStringLength() - (int)res.length();
            ASSERT(offset > 0);
            const char *tail = type.GetString() + offset;
            ASSERT(::strcmp(res.c_str(), tail) == 0);
          }
          #endif
        }

        addTemplate(i, templ["$name"].GetString(), comps, extends);
      }
    }
  }

  systemDescs.resize(reg_sys_count);

  for (const SystemDescription *sys = reg_sys_head; sys; sys = sys->next)
  {
    ASSERT(sys->sys != nullptr);
    ASSERT(find_component(sys->stageName) != nullptr);
    const_cast<SystemDescription*>(sys)->stageId = find_component(sys->stageName)->id;
    ASSERT(sys->stageId >= 0);

    systemDescs[sys->id] = sys;
    systems.push_back({ getSystemWeight(sys->name), sys });
  }

  eastl::sort(systems.begin(), systems.end(),
    [](const System &lhs, const System &rhs) { return lhs.weight < rhs.weight; });

  queries.resize(reg_sys_count);
  for (const auto &sys : systems)
  {
    auto &q = queries[sys.desc->id];
    q.stageId = sys.desc->stageId;
    q.sysId = sys.desc->id;
    q.desc = sys.desc->queryDesc;
    q.desc.filter = sys.desc->filter;
    q.name = sys.desc->name;
    ASSERT_FMT(q.desc.isValid() || sys.desc->mode == SystemDescription::Mode::FROM_EXTERNAL_QUERY, "Query for system '%s' is invalid!", sys.desc->name);

    for (int archetypeId = 0, sz = archetypes.size(); archetypeId < sz; ++archetypeId)
    {
      const auto &type = archetypes[archetypeId];
      if (not_have_components(type, q.desc.notHaveComponents) &&
          has_components(type, q.desc.components) &&
          has_components(type, q.desc.haveComponents))
      {
        q.desc.archetypes.push_back(archetypeId);
      }
    }
  }

  systemJobs.resize(systems.size());
  systemDependencies.resize(systems.size());

  for (int i = reg_sys_count - 1; i >= 0; --i)
  {
    const auto &qI = queries[systems[i].desc->id];
    const auto &compsI = systems[i].desc->queryDesc.components;
    auto &deps = systemDependencies[systems[i].desc->id];

    for (int j = i - 1; j >= 0; --j)
    {
      const auto &qJ = queries[systems[j].desc->id];

      // TODO: abort loop normaly instead of dry runs

      bool found = false;
      for (int archI : qI.desc.archetypes)
        for (int archJ : qJ.desc.archetypes)
          if (!found && archI == archJ)
          {
            found = true;
            break;
          }

      if (!found)
        continue;

      found = false;
      for (const auto &compI : compsI)
        for (const auto &compJ : systems[j].desc->queryDesc.components)
          if (!found && compI.name == compJ.name && ((compJ.flags & ComponentDescriptionFlags::kWrite) || (compI.flags & ComponentDescriptionFlags::kWrite)))
          {
            // TODO: Check query intersection!!!
            found = true;
            deps.push_back(systems[j].desc->id);
            break;
          }
    }
  }

  for (int i = 0, sz = systems.size(); i < sz; ++i)
  {
    DEBUG_LOG(systems[i].desc->name.str);
    if (systemDependencies[systems[i].desc->id].empty())
    {
      DEBUG_LOG("  []");
    }
    else
    {
      for (int dep : systemDependencies[systems[i].desc->id])
        DEBUG_LOG("  " << systemDescs[dep]->name.str);
    }
  }

  for (auto &sys : systems)
  {
    auto &q = queries[sys.desc->id];
    if (sys.desc->mode == SystemDescription::Mode::FROM_EXTERNAL_QUERY)
      q.desc = empty_query_desc;
  }

  for (const auto &sys : systems)
  {
    for (const auto &c : sys.desc->queryDesc.trackComponents)
      enableChangeDetection(c.name);
  }

  namedQueries.resize(reg_query_count);
  int queryIdx = 0;
  for (const PersistentQueryDescription *query = reg_query_head; query; query = query->next, ++queryIdx)
  {
    ASSERT(getQueryByName(query->name) == nullptr);
    namedQueries[queryIdx].name = query->name;
    namedQueries[queryIdx].desc = query->desc;
    namedQueries[queryIdx].desc.filter = query->filter;
    for (const auto &c : query->desc.trackComponents)
      enableChangeDetection(c.name);
  }

  namedIndices.resize(reg_index_count);
  int indexIdx = 0;
  for (const IndexDescription *index = reg_index_head; index; index = index->next, ++indexIdx)
  {
    ASSERT(getIndexByName(index->name) == nullptr);
    namedIndices[indexIdx].name = index->name;
    namedIndices[indexIdx].componentName = index->componentName;
    namedIndices[indexIdx].desc = index->desc;
    namedIndices[indexIdx].desc.filter = index->filter;

    enableChangeDetection(index->componentName);
  }
}

int EntityManager::getSystemId(const ConstHashedString &name) const
{
  for (int i = 0, sz = systemDescs.size(); i < sz; ++i)
    if (systemDescs[i]->name == name)
      return systemDescs[i]->id;
  return -1;
}

jobmanager::DependencyList EntityManager::getSystemDependencyList(int id) const
{
  jobmanager::DependencyList deps;
  for (int d : g_mgr->systemDependencies[id])
    if (g_mgr->systemJobs[d])
      deps.push_back(g_mgr->systemJobs[d]);
  return eastl::move(deps);
}

void EntityManager::waitSystemDependencies(int id) const
{
  for (int d : g_mgr->systemDependencies[id])
    jobmanager::wait(g_mgr->systemJobs[d]);
}

int EntityManager::getSystemWeight(const ConstHashedString &name) const
{
  auto res = eastl::find_if(order.begin(), order.end(), [name](const eastl::string &n) { return n == name.str; });
  ASSERT_FMT(res != order.end(), "System '%s' must be added to $order!", name);
  return (int)(res - order.begin());
}

const ComponentDescription* EntityManager::getComponentDescByName(const char *name) const
{
  auto res = componentDescByNames.find(hash_str(name));
  if (res == componentDescByNames.end())
    return nullptr;
  return res->second;
}

const ComponentDescription* EntityManager::getComponentDescByName(const HashedString &name) const
{
  auto res = componentDescByNames.find(name);
  if (res == componentDescByNames.end())
    return nullptr;
  return res->second;
}

const ComponentDescription* EntityManager::getComponentDescByName(const ConstHashedString &name) const
{
  auto res = componentDescByNames.find(name);
  if (res == componentDescByNames.end())
    return nullptr;
  return res->second;
}

Query* EntityManager::getQueryByName(const ConstHashedString &name)
{
  for (auto &q : namedQueries)
    if (q.name == name)
      return &q;
  return nullptr;
}

Index* EntityManager::getIndexByName(const ConstHashedString &name)
{
  for (auto &i : namedIndices)
    if (i.name == name)
      return &i;
  return nullptr;
}

static void add_component_to_template(const char *comp_type, const HashedString &comp_name,
  EntityTemplate &templ,
  eastl::hash_map<HashedString, const ComponentDescription*> &component_desc_by_names)
{
  auto res = eastl::find_if(templ.components.begin(), templ.components.end(), [&](const Component &c) { return c.name == comp_name; });
  if (res != templ.components.end())
    return;

  templ.components.push_back({ 0, comp_name, find_component(comp_type)->size, find_component(comp_type) });

  const ComponentDescription *desc = templ.components.back().desc;
  ASSERT(desc != nullptr);
  ASSERT(component_desc_by_names.find(comp_name) == component_desc_by_names.end() || component_desc_by_names[comp_name] == desc);
  component_desc_by_names[comp_name] = desc;
}

static void process_extends(EntityManager *mgr,
  EntityTemplate &templ,
  const eastl::vector<const char*> &extends,
  eastl::hash_map<HashedString, const ComponentDescription*> &component_desc_by_names)
{
  for (const auto &e : extends)
  {
    const int templateId = mgr->getTemplateId(e);
    templ.extends.push_back(templateId);
    for (const auto &c : mgr->templates[templateId].components)
      add_component_to_template(c.desc->name, c.name, templ, component_desc_by_names);
  }
}

void EntityManager::addTemplate(int doc_id, const char *templ_name, const eastl::vector<eastl::pair<const char*, const char*>> &comp_names, const eastl::vector<const char*> &extends)
{
  templates.emplace_back();
  auto &templ = templates.back();
  templ.docId = doc_id;
  templ.name = templ_name;

  process_extends(this, templ, extends, componentDescByNames);

  for (const auto &name : comp_names)
    add_component_to_template(name.first, name.second, templ, componentDescByNames);

  eastl::sort(templ.components.begin(), templ.components.end(),
    [](const Component &lhs, const Component &rhs)
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
  }
  templ.size = offset;

  // TODO: Create arhcetypes by componets list. Not per template
  Archetype &type = archetypes.emplace_back();
  type.storages.resize(templ.components.size());
  type.storageNames.resize(templ.components.size());

  templ.archetypeId = archetypes.size() - 1;

  int compIdx = 0;
  for (auto &storage : type.storages)
  {
    const auto &c = templ.components[compIdx];

    storage = componentDescByNames[c.name]->createStorage();
    ASSERT(storage != nullptr);

    type.storageNames[compIdx] = c.name;
    storage->elemSize = componentDescByNames[c.name]->size;

    ++compIdx;
  }

  type.storages.emplace_back() = eidComp->createStorage();
  type.storageNames.emplace_back() = HASH("eid");
  type.storages.back()->elemSize = eidComp->size;
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
  ASSERT(std::find_if(asyncValues.begin(), asyncValues.end(), [eid](const AsyncValue &v) { return v.eid == eid; }) == asyncValues.end());
  asyncValues.emplace_back(eid, std::move(value));
  if (eid.generation == entityGenerations[eid.index])
    entities[eid.index].ready = false;
}

int EntityManager::getTemplateId(const char *name)
{
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == name)
      return i;
  ASSERT_FMT(false, "Template '%s' not found!", name ? name : "(null)");
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

void EntityManager::buildComponentsValuesFromTemplate(int template_id, const JValue &comps, JFrameValue &out_value)
{
  auto &templ = templates[template_id];

  const JValue &value = templatesDoc["$templates"][templ.docId];

  JFrameAllocator allocator;
  out_value.CopyFrom(value["$components"], allocator);

  process_templates(this, templ, templatesDoc, out_value, allocator);

  if (!comps.IsNull())
    for (auto compIter = comps.MemberBegin(); compIter != comps.MemberEnd(); ++compIter)
      out_value[compIter->name.GetString()]["$value"].CopyFrom(compIter->value, allocator);
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
  ASSERT(templateId >= 0);

  auto &templ = templates[templateId];

  JFrameValue tmpValue(rapidjson::kObjectType);
  buildComponentsValuesFromTemplate(templateId, comps, tmpValue);

  int freeIndex = -1;

  static constexpr uint32_t MINIMUM_FREE_INDICES = 1024;
  if (freeEntityQueue.size() > MINIMUM_FREE_INDICES)
  {
    freeIndex = freeEntityQueue.front();
    freeEntityQueue.pop_front();
  }

  if (freeIndex < 0)
  {
    freeIndex = entities.size();
    entities.emplace_back();
    entityGenerations.push_back(0);
  }

  EntityId eid = make_eid(entityGenerations[freeIndex], freeIndex);

  auto &e = entities[freeIndex];

  e.templateId = templateId;
  e.archetypeId = templ.archetypeId;
  e.ready = true;

  ++entitiesCount;

  archetypes[e.archetypeId].entitiesCount++;
  if (archetypes[e.archetypeId].entitiesCount > archetypes[e.archetypeId].entitiesCapacity)
    archetypes[e.archetypeId].entitiesCapacity = archetypes[e.archetypeId].entitiesCount;

  int compId = 0;
  auto &type = archetypes[templ.archetypeId];
  for (const auto &c : templ.components)
  {
    uint8_t *mem = nullptr;
    int offset = 0;
    eastl::tie(mem, offset) = type.storages[compId++]->allocate();

    c.desc->init(mem, tmpValue[c.name.str]);
  }

  // eid
  {
    uint8_t *mem = nullptr;
    int offset = 0;
    eastl::tie(mem, offset) = type.storages.back()->allocate();
    new (mem) EntityId(eid);

    e.indexInArchetype = offset / sizeof(EntityId);
  }

  sendEventSync(eid, EventOnEntityCreate{});

  return eid;
}

void EntityManager::tick()
{
  jobmanager::wait_all_jobs();

  for (auto &job : g_mgr->systemJobs)
    job = jobmanager::JobId{};

  bool shouldInvalidateQueries = false;

  while (!deleteQueue.empty())
  {
    EntityId eid = deleteQueue.front();

    auto &entity = entities[eid.index];
    if (eid.generation == entityGenerations[eid.index])
    {
      shouldInvalidateQueries = true;

      const auto &templ = templates[entity.templateId];

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

      --entitiesCount;
      --type.entitiesCount;

      entityGenerations[eid.index] = (eid.generation + 1) % EntityId::GENERATION_LIMIT;
      entity.ready = false;
      entity.indexInArchetype = -1;

      freeEntityQueue.push_back(eid.index);
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
    for (auto &t : archetypes)
      t.invalidate();

  for (const auto &v : asyncValues)
  {
    const bool ready = v.isReady();
    if (ready)
    {
      shouldInvalidateQueries = true;

      if (v.eid.generation == entityGenerations[v.eid.index])
      {
        entities[v.eid.index].ready = ready;
        sendEventSync(v.eid, EventOnEntityReady{});
      }
    }
  }

  if (!asyncValues.empty())
    asyncValues.erase(eastl::remove_if(asyncValues.begin(), asyncValues.end(), [](const AsyncValue &v) { return v.isReady(); }), asyncValues.end());

  bool queriesInvalidated = false;
  if (shouldInvalidateQueries)
  {
    queriesInvalidated = true;
    for (auto &q : queries)
      performQuery(q);
    for (auto &q : namedQueries)
      performQuery(q);
    for (auto &i : namedIndices)
      rebuildIndex(i);
  }
  else
  {
    for (auto &q : queries)
      if (q.dirty)
      {
        queriesInvalidated = true;
        performQuery(q);
      }
    for (auto &q : namedQueries)
      if (q.dirty)
      {
        queriesInvalidated = true;
        performQuery(q);
      }
    for (auto &i : namedIndices)
      if (i.dirty)
        rebuildIndex(i);
  }

  // TODO: Perform only queries are depent on changed component
  // if (queriesInvalidated)
  if (shouldInvalidateQueries)
    sendEventBroadcastSync(EventOnChangeDetected{});

  const int streamIndex = currentEventStream;
  currentEventStream = (currentEventStream + 1) % events.size();
  ASSERT(currentEventStream != streamIndex);

  {
    FrameSnapshot snapshot;
    fillFrameSnapshot(snapshot);

    while (events[streamIndex].count)
    {
      EventStream::Header header;
      RawArg ev;
      eastl::tie(header, ev) = events[streamIndex].pop();

      if (header.flags & EventStream::kBroadcast)
      {
        sendEventBroadcastSync(header.eventId, ev);
        tick(DispatchBroadcastEventStage{ header.eventId, ev });
      }
      else
      {
        sendEventSync(header.eid, header.eventId, ev);
        tick(DispatchEventStage{ header.eid, header.eventId, ev });
      }
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
  for (int i = 0; i < (int)storageNames.size(); ++i)
    if (storageNames[i] == name)
      return i;
  return -1;
}

int Archetype::getComponentIndex(const ConstHashedString &name) const
{
  return getComponentIndex(HashedString(name));
}

template <typename T>
inline static bool is_components_values_equal_to(const T &value, int entity_idx, const Archetype &type, const eastl::vector<Component> &components)
{
  for (const auto &c : components)
  {
    const int compIdx = type.getComponentIndex(c.name);
    if (type.storages[compIdx]->getByIndex<T>(entity_idx) != value)
      return false;
  }
  return true;
}

Query* Index::find(uint32_t value)
{
  Item item = { -1, value };
  auto res = eastl::lower_bound(items.begin(), items.end(), item);
  return res != items.end() && *res == item ? &queries[res->queryId] : nullptr;
}

void Query::addChunks(const QueryDescription &in_desc, Archetype &type, int begin, int entities_count)
{
  ++chunksCount;

  entitiesCount += entities_count;
  chunks.resize(chunks.size() + componentsCount);
  entitiesInChunk.resize(chunksCount);
  entitiesInChunk[chunksCount - 1] = entities_count;

  int compIdx = 0;
  for (const auto &c : in_desc.components)
  {
    auto &storage = type.storages[type.getComponentIndex(c.name)];

    QueryChunk chunk;
    chunk.beginData = storage->getRawByIndex(begin);
    chunk.endData = storage->getRawByIndex(begin + entities_count);
    chunk.elemSize = c.size;

    chunks[compIdx + (chunksCount - 1) * componentsCount] = chunk;
    compIdx++;
  }

  auto &storage = type.storages[type.getComponentIndex(hash::cstr("eid"))];

  QueryChunk chunk;
  chunk.beginData = storage->getRawByIndex(begin);
  chunk.endData = storage->getRawByIndex(begin + entities_count);
  chunk.elemSize = sizeof(EntityId);
  chunks[compIdx + (chunksCount - 1) * componentsCount] = chunk;
}

void EntityManager::performQuery(Query &query)
{
  const bool isValid = query.desc.isValid();

  query.dirty = false;
  query.chunksCount = 0;
  query.entitiesCount = 0;
  query.chunks.clear();
  query.entitiesInChunk.clear();
  query.componentsCount = query.desc.components.size() + 1;

  for (auto &type : archetypes)
  {
    // TODO: ASSERT on type mismatch
    bool ok =
      isValid &&
      not_have_components(type, query.desc.notHaveComponents) &&
      has_components(type, query.desc.components) &&
      has_components(type, query.desc.haveComponents);

    if (ok)
    {
      ASSERT(type.entitiesCapacity == type.storages[0]->totalCount);

      int begin = -1;
      for (int i = 0; i < type.entitiesCapacity; ++i)
      {
        // TODO: Find a better solution
        ok = !type.storages[0]->freeMask[i];

        ok = ok && (!query.desc.filter || query.desc.filter(type, i));

        if (ok && begin < 0)
          begin = i;

        if (begin >= 0 && (!ok || i == type.entitiesCapacity - 1))
        {
          query.addChunks(query.desc, type, begin, i - begin + (ok ? 1 : 0));
          begin = -1;
        }
      }
    }
  }
}

void EntityManager::rebuildIndex(Index &index)
{
  const bool isValid = index.desc.isValid();

  index.queries.clear();
  index.items.clear();

  for (auto &type : archetypes)
  {
    // TODO: ASSERT on type mismatch
    bool ok =
      isValid &&
      not_have_components(type, index.desc.notHaveComponents) &&
      has_components(type, index.desc.components) &&
      has_components(type, index.desc.haveComponents);

    if (ok)
    {
      ASSERT(type.entitiesCapacity == type.storages[0]->totalCount);

      const int componentIdx = type.getComponentIndex(index.componentName);
      ASSERT(componentIdx >= 0);
      const int componentSize = type.storages[componentIdx]->elemSize;
      ASSERT(componentSize == sizeof(Index::Item::value));

      int lastQueryId = -1;
      int begin = -1;
      for (int i = 0; i < type.entitiesCapacity; ++i)
      {
        // TODO: Find a better solution
        ok = !type.storages[0]->freeMask[i];

        if (!ok && lastQueryId < 0)
          continue;

        int queryId = lastQueryId;
        Query *query = queryId >= 0 ? &index.queries[queryId] : nullptr;

        if (ok)
        {
          Index::Item item = { -1, type.storages[componentIdx]->getByIndex<uint32_t>(i) };
          auto res = eastl::lower_bound(index.items.begin(), index.items.end(), item);
          if (res == index.items.end())
          {
            item.queryId = (int)index.queries.size();
            query = &index.queries.emplace_back();
            queryId = item.queryId;

            index.items.push_back(eastl::move(item));
          }
          else if (*res == item)
          {
            ASSERT(res->queryId >= 0 && res->queryId < (int)index.queries.size());
            item.queryId = res->queryId;
            query = &index.queries[res->queryId];
            queryId = item.queryId;
          }
          else
          {
            item.queryId = (int)index.queries.size();
            query = &index.queries.emplace_back();
            queryId = item.queryId;

            index.items.insert(res, eastl::move(item));
          }

          if (lastQueryId >= 0 && lastQueryId != queryId && begin >= 0)
          {
            ASSERT(begin >= 0 && i > begin);
            index.queries[lastQueryId].addChunks(index.desc, type, begin, i - begin);
            begin = -1;
          }
        }

        ASSERT(query != nullptr);

        query->componentsCount = index.desc.components.size() + 1;
        lastQueryId = queryId;

        ok = ok && (!index.desc.filter || index.desc.filter(type, i));

        if (ok && begin < 0)
          begin = i;

        if (begin >= 0 && (!ok || i == type.entitiesCapacity - 1))
        {
          ASSERT(begin >= 0 && (i + (ok ? 1 : 0)) > begin);
          query->addChunks(index.desc, type, begin, i - begin + (ok ? 1 : 0));
          begin = -1;
        }
      }
    }
  }
}

void EntityManager::fillFrameSnapshot(FrameSnapshot &snapshot) const
{
  // TODO: Optimize
  // TODO: Store component copies as normal component
  // TODO: Just do noraml compare in a loop like system call
  for (const auto &type : archetypes)
  {
    for (const auto &name : trackComponents)
    {
      int index = type.getComponentIndex(name);
      if (index >= 0)
      {
        const size_t sz = type.storages[index]->size();
        snapshot.emplace_back() = alloc_frame_mem(sz);
        ::memcpy(snapshot.back(), type.storages[index]->data(), sz);
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
        if (::memcmp(snapshot[i++], type.storages[index]->data(), type.storages[index]->size()))
        {
          for (auto &q : queries)
            if (q.desc.isDependOnComponent(name))
              q.dirty = true;
          for (auto &q : namedQueries)
            if (q.desc.isDependOnComponent(name))
              q.dirty = true;
          for (auto &i : namedIndices)
            if (i.desc.isDependOnComponent(name))
              i.dirty = true;
          // break;
          // TODO: Correct way to interrupt the cycle
          return;
        }
      }
    }
  }
}

void EntityManager::tickStage(int stage_id, const RawArg &stage)
{
  FrameSnapshot snapshot;
  fillFrameSnapshot(snapshot);

  for (const auto &sys : systems)
    if (sys.desc->stageId == stage_id)
      sys.desc->sys(stage, queries[sys.desc->id]);

  checkFrameSnapshot(snapshot);
}

void EntityManager::sendEvent(EntityId eid, int event_id, const RawArg &ev)
{
  ASSERT(eid);
  events[currentEventStream].push(eid, EventStream::kTarget, event_id, ev);
}

void EntityManager::sendEventSync(EntityId eid, int event_id, const RawArg &ev)
{
  if (eid.generation != entityGenerations[eid.index])
    return;

  auto &e = entities[eid.index];
  auto &type = archetypes[e.archetypeId];

  for (const auto &sys : systems)
    if (sys.desc->stageId == event_id)
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

        sys.desc->sys(ev, query);
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
    if (sys.desc->stageId == event_id)
    {
      auto &query = queries[sys.desc->id];
      sys.desc->sys(ev, query);
    }
}

void EntityManager::enableChangeDetection(const HashedString &name)
{
  if (name == hash::cstr("eid"))
    return;
  DEBUG_LOG("enableChangeDetection: " << name.str);
  trackComponents.insert(name);
}

void EntityManager::disableChangeDetection(const HashedString &name)
{
  DEBUG_LOG("disableChangeDetection: " << name.str);
  trackComponents.erase(name);
}