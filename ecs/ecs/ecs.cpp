#include "ecs.h"
#include "autoBind.h"

#include <sstream>

EntityManager *g_mgr = nullptr;

const SystemDescription *SystemDescription::head = nullptr;
int SystemDescription::count = 0;

const ComponentDescription *ComponentDescription::head = nullptr;
int ComponentDescription::count = 0;

const PersistentQueryDescription *PersistentQueryDescription::head = nullptr;
int PersistentQueryDescription::count = 0;

const IndexDescription *IndexDescription::head = nullptr;
int IndexDescription::count = 0;

const AutoBindDescription *AutoBindDescription::head = nullptr;
int AutoBindDescription::count = 0;

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

const SystemDescription *find_system(const ConstHashedString &name)
{
  for (const auto *sys = SystemDescription::head; sys; sys = sys->next)
    if (sys->name == name)
      return sys;
  return nullptr;
}

ComponentDescription::ComponentDescription(const char *_name, uint32_t _type_hash, uint32_t _size) :
  id(ComponentDescription::count),
  typeHash(_type_hash),
  size(_size)
{
  name = ::_strdup(_name);
  next = ComponentDescription::head;
  ComponentDescription::head = this;
  ++ComponentDescription::count;
}

ComponentDescription::~ComponentDescription()
{
  ::free(name);
}

const ComponentDescription *find_component(const char *name)
{
  for (const auto *comp = ComponentDescription::head; comp; comp = comp->next)
    if (::strcmp(comp->name, name) == 0)
      return comp;
  return nullptr;
}

const ComponentDescription *find_component(const ConstHashedString &name)
{
  return find_component(name.hash);
}

const ComponentDescription *find_component(uint32_t type_hash)
{
  for (const auto *comp = ComponentDescription::head; comp; comp = comp->next)
    if (comp->typeHash == type_hash)
      return comp;
  return nullptr;
}

void do_auto_bind_module(const ConstHashedString &module_name, das::Module &module, das::ModuleLibrary &lib)
{
  for (const auto *autoBind = AutoBindDescription::head; autoBind; autoBind = autoBind->next)
    if (autoBind->module == module_name)
      autoBind->callback(module, lib);
}

PersistentQueryDescription::PersistentQueryDescription(const ConstHashedString &_name, const ConstQueryDescription &_desc, filter_t &&f) : name(_name), desc(_desc), filter(eastl::move(f))
{
  next = PersistentQueryDescription::head;
  PersistentQueryDescription::head = this;
  ++PersistentQueryDescription::count;
}

IndexDescription::IndexDescription(const ConstHashedString &_name, const ConstHashedString &component_name, const ConstQueryDescription &_desc, filter_t &&f) : name(_name), componentName(component_name), desc(_desc), filter(eastl::move(f))
{
  next = IndexDescription::head;
  IndexDescription::head = this;
  ++IndexDescription::count;
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

void EntityManager::findArchetypes(QueryDescription &desc)
{
  if (!desc.isValid())
    return;

  desc.archetypes.clear();

  for (int archetypeId = 0, sz = archetypes.size(); archetypeId < sz; ++archetypeId)
  {
    const auto &type = archetypes[archetypeId];
    if (not_have_components(type, desc.notHaveComponents) &&
        has_components(type, desc.components) &&
        has_components(type, desc.haveComponents))
    {
      desc.archetypes.push_back(archetypeId);
    }
  }
}

template<class MarkContainer, class ListContainer, class EdgeContainer, typename LoopDetected>
static bool visit_top_sort(size_t node, const EdgeContainer &edges, MarkContainer & temp, MarkContainer &perm, ListContainer& result, LoopDetected cb)
{
  if (perm[node])
    return true;
  if (temp[node])
  {
    cb(node, temp);
    perm.set(node, true);
    return false;
  }
  temp.set(node, true);
  bool ret = true;
  if (edges.size() > node)
    for (auto child : edges[node])
      ret &= visit_top_sort(child, edges, temp, perm, result, cb);

  temp.set(node, false);
  if (!perm[node])//this check is needed in case graph is not DAG. we will just ignore such nodes.
    result.push_back(node);
  perm.set(node, true);
  return ret;
}

typedef eastl::fixed_vector<int, 2, true> edge_container;//typically no more than 2 edges

template<typename LoopDetected>
static bool topo_sort(size_t N, const eastl::vector<edge_container> &edges, eastl::vector<int, FrameMemAllocator> &sortedList, LoopDetected cb)
{
  sortedList.reserve(N);
  eastl::bitvector<FrameMemAllocator> tempMark(N, false);
  eastl::bitvector<FrameMemAllocator> visitedMark(N, false);
  bool isDAG = true;
  for (size_t i = 0; i < N; ++i)
    isDAG &= visit_top_sort(i, edges, tempMark, visitedMark, sortedList, cb);
  return isDAG;
}

static eastl::vector<eastl::string_view, FrameMemAllocator> split(const char *str, const char *delim)
{
  if (!str || str[0] == '\0')
    return {""};

  eastl::vector<eastl::string_view, FrameMemAllocator> elems;

  const char *begin = str;
  const char *end = ::strstr(begin, delim);
  if (!end)
    return {str};

  for (; end; begin = end + 1, end = ::strstr(begin, delim))
    elems.emplace_back(begin, end - begin);

  const int len = ::strlen(str);
  if (begin != str)
    elems.emplace_back(begin, str - begin + len);

  return elems;
}

void EntityManager::init()
{
  jobmanager::init();

  // Reserve eid = 0 as invalid
  entities.resize(1);

  // No more data/templates.json! daScript only!

  systemDescs.resize(SystemDescription::count);

  for (const auto *sys = SystemDescription::head; sys; sys = sys->next)
  {
    // Not valid since we have Barriers
    // ASSERT(sys->sys != nullptr);
    systemDescs[sys->id] = sys;
    systems.push_back({ sys->sys, sys });
  }

  eastl::hash_map<eastl::string_view, int, eastl::hash<eastl::string_view>, eastl::equal_to<eastl::string_view>, FrameMemAllocator> nameESMap;

  eastl::vector<int> esToGraphNodeMap;//ES to graph vertex map
  esToGraphNodeMap.reserve(systems.size());
  eastl::vector<int> graphNodeToEsMap;//graph vertex -> ES map
  graphNodeToEsMap.reserve(systems.size());
  int graphNodesCount = 0;
  eastl::vector<edge_container> edgesFrom;
  auto insertEdge = [&edgesFrom](int from, int to)
  {
    if (int(edgesFrom.size()) <= eastl::max(from, to))
      edgesFrom.resize(eastl::max(from, to)+1);
    edgesFrom[from].push_back(to);
  };

  // auto r1 = split("", ",");
  // auto r2 = split("aaa", ",");
  // auto r3 = split("aaa,bbb", ",");
  // auto r4 = split("aaa,bbb,ccc", ",");
  // auto r5 = split("aaa,,ccc", ",");
  // auto r6 = split(",aaa", ",");
  // auto r7 = split("aaa,", ",");

  //before/after edges
  auto insertEdges = [&](const char *name, int graphNode, const char *nodes, bool before)
  {
    if (!nodes || nodes[0] == '*')
      return;
    for (const auto &node : split(nodes, ","))
    {
      auto res = nameESMap.emplace(node, graphNodesCount);
      if (res.second)
        ++graphNodesCount;
      const int other = res.first->second;
      insertEdge(before ? graphNode : other, before ? other : graphNode);
    }
  };

  for (int i = 0, sz = systems.size(); i < sz; ++i)
  {
    eastl::string_view name(systems[i].desc->name.str);
    auto insResult = nameESMap.emplace(name, graphNodesCount);
    const int graphNode = insResult.first->second;
    if (!insResult.second)
    {
      const int j = int(graphNodeToEsMap.size()) <= graphNode ? -1 : graphNodeToEsMap[graphNode];
      ASSERT(j < 0);
    }
    else
      graphNodesCount++;

    if (int(graphNodeToEsMap.size()) <= graphNode)
      graphNodeToEsMap.resize(graphNode + 1, -1);
    graphNodeToEsMap[graphNode] = i;

    if (int(esToGraphNodeMap.size()) <= i)
      esToGraphNodeMap.resize(i + 1, -1);
    esToGraphNodeMap[i] = graphNode;
  }

  for (int i = 0, sz = systems.size(); i < sz; ++i)
  {
    auto it = nameESMap.find(eastl::string_view(systems[i].desc->name.str));
    ASSERT(it != nameESMap.end());
    const int graphNode = it->second;
    insertEdges(systems[i].desc->name.str, graphNode, systems[i].desc->before.c_str(), true);
    insertEdges(systems[i].desc->name.str, graphNode, systems[i].desc->after.c_str(), false);
  }

  eastl::vector<int, FrameMemAllocator> sortedList;
  auto loopDetected = [&](int i, auto &)
  {
    auto it = eastl::find_if(nameESMap.begin(), nameESMap.end(), [&](const auto &it){return it.second == i;});
    eastl::string node="n/a";
    if (it != nameESMap.end())
      node = it->first;
    ASSERT_FMT(false, "Node %s resulted in graph to become cyclic and was removed from sorting. ES order is non-determinstic", node.c_str());
  };
  topo_sort(graphNodesCount, edgesFrom, sortedList, loopDetected);

  const int lowestWeight = eastl::numeric_limits<int>::max();
  eastl::vector<int, FrameMemAllocator> sortedWeights;
  sortedWeights.resize(sortedList.size(), lowestWeight);
  for (size_t i = 0, sz = sortedList.size(); i < sz; ++i)
    if (uint32_t(sortedList[i]) < sortedList.size())
      sortedWeights[sortedList[i]] = sortedList.size()-i;

  struct SystemWithWeight
  {
    int id;
    int weight;
    SystemWithWeight(int _id, int _weight) : id(_id), weight(_weight) {}
  };
  eastl::vector<SystemWithWeight, FrameMemAllocator> weights;
  weights.reserve(systems.size());

  for (int i = 0, sz = systems.size(); i < sz; ++i)
  {
    const System &sys = systems[i];

    const uint32_t graphNode = i < int(esToGraphNodeMap.size()) ? esToGraphNodeMap[i] : ~0u;
    const int weight = (uint32_t(graphNode) < sortedWeights.size()) ? sortedWeights[graphNode] : lowestWeight;

    weights.push_back({i, weight});
  }

  eastl::sort(weights.begin(), weights.end(), [](const SystemWithWeight &a, const SystemWithWeight&b) {return a.weight < b.weight;});
  eastl::vector<System> systemsSorted;
  systemsSorted.resize(systems.size());
  for (int i = 0, sz = weights.size(); i < sz; ++i)
  {
    systemsSorted[i] = systems[weights[i].id];
    const bool isBarrier = systemsSorted[i].sys == nullptr;
    DEBUG_LOG((!isBarrier ? "  " : "") << systemsSorted[i].desc->name.str);
  }
  eastl::swap(systemsSorted, systems);

  for (int i = 0, sz = systems.size(); i < sz; ++i)
    systemsByStage.insert(eastl::pair<uint32_t, int>(systems[i].desc->stageName.hash, i));

  for (auto &sys : systems)
    sys.queryId = createQuery(sys.desc->name, sys.desc->queryDesc, sys.desc->filter);

  systemJobs.resize(systems.size());
  systemDependencies.resize(systems.size());

  // TODO: Check this. The code might be broken. archetypes are empty by this moment.
  for (int i = SystemDescription::count - 1; i >= 0; --i)
  {
    const auto &qI = queryDescriptions[systems[i].queryId.index];
    const auto &compsI = systems[i].desc->queryDesc.components;
    auto &deps = systemDependencies[systems[i].desc->id];

    for (int j = i - 1; j >= 0; --j)
    {
      const auto &qJ = queryDescriptions[systems[j].queryId.index];

      // TODO: abort loop normaly instead of dry runs

      bool found = false;
      for (int archI : qI.archetypes)
        for (int archJ : qJ.archetypes)
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
    if (sys.desc->mode == SystemDescription::Mode::FROM_EXTERNAL_QUERY)
    {
      deleteQuery(sys.queryId);
      sys.queryId = QueryId();
    }

  for (const auto &sys : systems)
  {
    for (const auto &c : sys.desc->queryDesc.trackComponents)
      enableChangeDetection(c.name);
  }

  int queryIdx = 0;
  for (const auto *query = PersistentQueryDescription::head; query; query = query->next, ++queryIdx)
  {
    const_cast<PersistentQueryDescription*>(query)->queryId = createQuery(query->name, query->desc, query->filter);
    for (const auto &c : query->desc.trackComponents)
      enableChangeDetection(c.name);
  }

  namedIndices.resize(IndexDescription::count);
  int indexIdx = 0;
  for (const auto *index = IndexDescription::head; index; index = index->next, ++indexIdx)
  {
    ASSERT(findIndex(index->name) == nullptr);
    namedIndices[indexIdx].name = index->name;
    namedIndices[indexIdx].componentName = index->componentName;
    namedIndices[indexIdx].desc = index->desc;
    namedIndices[indexIdx].desc.filter = index->filter;
    findArchetypes(namedIndices[indexIdx].desc);
    enableChangeDetection(index->componentName);
  }

  dirtyQueries.reserve(queries.size());
  dirtyNamedIndices.reserve(namedIndices.size());
}

void EntityManager::registerSystem(const ConstHashedString &name, SystemDescription::SystemCallback callback)
{
  // Insert to systems
  // Sort systems
  // Insert to systemsByStage
  // Insert to queries

  // Do not shrik systems array. id only move forward.
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
  for (int d : systemDependencies[id])
    if (systemJobs[d])
      deps.push_back(systemJobs[d]);
  return eastl::move(deps);
}

void EntityManager::waitSystemDependencies(int id) const
{
  for (int d : systemDependencies[id])
    jobmanager::wait(systemJobs[d]);
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

Index* EntityManager::findIndex(const ConstHashedString &name)
{
  for (auto &i : namedIndices)
    if (i.name == name)
      return &i;
  return nullptr;
}

void EntityManager::addTemplate(const char *templ_name, ComponentsMap &&cmap)
{
  EntityTemplate &templ = templates.emplace_back();
  templ.name = templ_name;
  templ.cmap = eastl::move(cmap);
  templ.cmap.createComponent(HASH("eid"), find_component(HASH("EntityId")));

  templ.size = 0;
  for (const auto &kv : templ.cmap.components)
  {
    templ.size += kv.second.desc->size;

    auto res = componentDescByNames.find(kv.first);
    if (res == componentDescByNames.end())
      componentDescByNames[kv.first] = kv.second.desc;
    else
    {
      ASSERT(res->second == kv.second.desc);
    }
  }

  // TODO: Create arhcetypes by componets list. Not per template
  Archetype &type = archetypes.emplace_back();
  type.componentsCount = templ.cmap.components.size();
  type.storages.reset(new Archetype::Storage[type.componentsCount]);
  type.storageNames.reset(new HashedString[type.componentsCount]);

  templ.archetypeId = archetypes.size() - 1;

  int i = 0;
  for (auto &kv : templ.cmap.components)
  {
    ASSERT(kv.second.desc != nullptr);
    new (&type.storages[i]) Archetype::Storage(kv.second.desc);
    new (&type.storageNames[i]) HashedString(kv.first);
    ++i;
  }

  // TODO: Do it once per tick
  for (QueryDescription &d : queryDescriptions)
    findArchetypes(d);
}

void EntityManager::createEntity(const char *templ_name, ComponentsMap &&comps)
{
  CreateQueueData q;
  q.templanemName = templ_name;
  q.components = eastl::move(comps);
  createQueue.emplace(eastl::move(q));
}

void EntityManager::deleteEntity(const EntityId &eid)
{
  deleteQueue.emplace(eid);
}

void EntityManager::waitFor(EntityId eid, std::future<bool> && value)
{
  ASSERT(std::find_if(asyncValues.begin(), asyncValues.end(), [eid](const AsyncValue &v) { return v.eid == eid; }) == asyncValues.end());
  asyncValues.emplace_back(eid, std::move(value));
  if (eidFactory.isValid(eid))
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

EntityId EntityManager::createEntitySync(const char *templ_name, ComponentsMap &&comps)
{
  int templateId = -1;
  // TODO: Use hash_map for search templates
  for (size_t i = 0; i < templates.size(); ++i)
    if (templates[i].name == templ_name)
    {
      templateId = (int)i;
      break;
    }
  ASSERT(templateId >= 0);

  auto &templ = templates[templateId];

  const EntityId eid = eidFactory.allocate();
  if (eid.index >= entities.size())
    entities.resize(eid.index + 1);

  DEBUG_LOG("[create][" << eid.handle << "]: " << templ_name);

  ASSERT(eid);

  auto &e = entities[eid.index];

  e.templateId = templateId;
  e.archetypeId = templ.archetypeId;
  e.ready = true;

  ++entitiesCount;

  comps.add(HASH("eid"), eid);
  e.indexInArchetype = archetypes[e.archetypeId].allocate(templ.cmap, eastl::move(comps));

  sendEventSync(eid, EventOnEntityCreate{});

  return eid;
}

void EntityManager::tick()
{
  jobmanager::wait_all_jobs();

  for (auto &job : systemJobs)
    job = jobmanager::JobId{};

  bool shouldInvalidateQueries = false;

  while (!deleteQueue.empty())
  {
    EntityId eid = deleteQueue.front();

    if (eidFactory.isValid(eid))
    {
      DEBUG_LOG("[delete][" << eid.handle << "]: " << templates[entities[eid.index].templateId].name.c_str());

      shouldInvalidateQueries = true;

      sendEventSync(eid, EventOnEntityDelete{});

      auto &entity = entities[eid.index];

      archetypes[entity.archetypeId].deallocate(entity.indexInArchetype);

      --entitiesCount;

      entity.ready = false;
      entity.indexInArchetype = -1;

      eidFactory.free(eid);
    }

    deleteQueue.pop();
  }

  while (!createQueue.empty())
  {
    shouldInvalidateQueries = true;

    auto &q = createQueue.front();
    createEntitySync(q.templanemName.c_str(), eastl::move(q.components));
    createQueue.pop();
  }

  for (const auto &v : asyncValues)
  {
    const bool ready = v.isReady();
    if (ready)
    {
      shouldInvalidateQueries = true;

      if (eidFactory.isValid(v.eid))
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
      performQuery(queryDescriptions[q.id.index], q);
    for (auto &i : namedIndices)
      rebuildIndex(i);
  }
  else
  {
    queriesInvalidated = !dirtyQueries.empty();
    for (QueryId queryId : dirtyQueries)
      performQuery(queryId);
    for (int indexIdx : dirtyNamedIndices)
      rebuildIndex(namedIndices[indexIdx]);

    dirtyQueries.clear();
    dirtyNamedIndices.clear();
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
        sendEventBroadcastSync(header.eventId, ev);
      else
        sendEventSync(header.eid, header.eventId, ev);
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
  for (int i = 0; i < componentsCount; ++i)
    if (storageNames[i] == name)
      return i;
  return -1;
}

int Archetype::getComponentIndex(const ConstHashedString &name) const
{
  return getComponentIndex(HashedString(name));
}

Query* Index::find(uint32_t value)
{
  auto res = itemsMap.find(value);
  return res != itemsMap.end() ? &queries[res->second] : nullptr;
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
    chunks[compIdx + (chunksCount - 1) * componentsCount] = type.getRaw(begin, type.getComponentIndex(c.name));
    compIdx++;
  }
}

void EntityManager::performQuery(const QueryId &qid)
{
  if (qidFactory.isValid(qid))
    performQuery(queryDescriptions[qid.index], queries[qid.index]);
}

void EntityManager::performQuery(const QueryDescription &desc, Query &query)
{
  const bool isValid = desc.isValid();

  query.chunksCount = 0;
  query.entitiesCount = 0;
  query.chunks.clear();
  query.entitiesInChunk.clear();
  query.componentsCount = desc.components.size();

  for (int archetypeId : desc.archetypes)
  {
    auto &type = archetypes[archetypeId];

    // TODO: ASSERT on type mismatch
    ASSERT(isValid && not_have_components(type, desc.notHaveComponents) && has_components(type, desc.components) && has_components(type, desc.haveComponents));

    bool ok = true;
    int begin = -1;
    for (int i = 0; i < type.entitiesCapacity; ++i)
    {
      // TODO: Find a better solution
      ok = !type.freeMask[i];

      ok = ok && (!desc.filter || desc.filter(type, i));

      if (ok && begin < 0)
        begin = i;

      if (begin >= 0 && (!ok || i == type.entitiesCapacity - 1))
      {
        query.addChunks(desc, type, begin, i - begin + (ok ? 1 : 0));
        begin = -1;
      }
    }
  }
}

void EntityManager::rebuildIndex(Index &index)
{
  const bool isValid = index.desc.isValid();

  index.queries.clear();
  index.itemsMap.clear();

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
      const int componentIdx = type.getComponentIndex(index.componentName);
      ASSERT(componentIdx >= 0);
      const int componentSize = type.storages[componentIdx].itemSize;
      ASSERT(componentSize == sizeof(uint32_t));

      int lastQueryId = -1;
      int begin = -1;
      for (int i = 0; i < type.entitiesCapacity; ++i)
      {
        // TODO: Find a better solution
        ok = !type.freeMask[i];

        if (!ok && lastQueryId < 0)
          continue;

        int queryId = lastQueryId;
        Query *query = queryId >= 0 ? &index.queries[queryId] : nullptr;

        if (ok)
        {
          const uint32_t key = type.get<uint32_t>(i, componentIdx);

          auto res = index.itemsMap.find(key);
          if (res == index.itemsMap.end() || res->first != key)
          {
            queryId = index.queries.size();
            query = &index.queries.emplace_back();

            index.itemsMap.insert(eastl::pair<uint32_t, int>(key, queryId));
          }
          else
          {
            ASSERT(res->second >= 0 && res->second < (int)index.queries.size());
            queryId = res->second;
            query = &index.queries[res->second];
          }

          if (lastQueryId >= 0 && lastQueryId != queryId && begin >= 0)
          {
            ASSERT(begin >= 0 && i > begin);
            index.queries[lastQueryId].addChunks(index.desc, type, begin, i - begin);
            begin = -1;
          }
        }

        ASSERT(query != nullptr);

        query->componentsCount = index.desc.components.size();
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
        // TODO: Normal copy. Check copy flags.
        const size_t sz = type.storages[index].size();
        snapshot.emplace_back() = alloc_frame_mem(sz);
        ::memcpy(snapshot.back(), type.storages[index].items, sz);
      }
    }
  }
}

void EntityManager::checkFrameSnapshot(const FrameSnapshot &snapshot)
{
  // TODO: Optimize
  // TODO: Use component's opertator== instead of ::memcmp if possible
  int i = 0;
  for (const auto &type : archetypes)
  {
    for (const auto &name : trackComponents)
    {
      int index = type.getComponentIndex(name);
      if (index >= 0)
      {
        if (::memcmp(snapshot[i++], type.storages[index].items, type.storages[index].size()))
        {
          for (const Query &q : queries)
            if (queryDescriptions[q.id.index].isDependOnComponent(name))
              dirtyQueries.push_back(q.id);
          for (int j = 0, sz = namedIndices.size(); j < sz; ++j)
            if (namedIndices[j].desc.isDependOnComponent(name))
              dirtyNamedIndices.push_back(j);
          // break;
          // TODO: Correct way to interrupt the cycle
          // return;
        }
      }
    }
  }
}

void EntityManager::sendEvent(EntityId eid, uint32_t event_id, const RawArg &ev)
{
  ASSERT(eid);
  events[currentEventStream].push(eid, EventStream::kUnicast, event_id, ev);
}

void EntityManager::sendEventSync(EntityId eid, uint32_t event_id, const RawArg &ev)
{
  if (!eidFactory.isValid(eid))
    return;

  auto &e = entities[eid.index];
  auto &type = archetypes[e.archetypeId];

  for (const auto &sys : systems)
    if (sys.desc->stageName.hash == event_id)
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
        query.componentsCount = sys.desc->queryDesc.components.size();
        query.addChunks(sys.desc->queryDesc, type, e.indexInArchetype, 1);

        sys.desc->sys(ev, query);
      }
    }
}

void EntityManager::sendEventBroadcast(uint32_t event_id, const RawArg &ev)
{
  events[currentEventStream].push(EntityId{}, EventStream::kBroadcast, event_id, ev);
}

void EntityManager::sendEventBroadcastSync(uint32_t event_id, const RawArg &ev)
{
  auto res = systemsByStage.equal_range(event_id);
  for (auto sysIt = res.first; sysIt != res.second; ++sysIt)
    systems[sysIt->second].sys(ev, queries[systems[sysIt->second].queryId.index]);
}

void EntityManager::invokeEventBroadcast(uint32_t event_id, const RawArg &ev)
{
  FrameSnapshot snapshot;
  fillFrameSnapshot(snapshot);

  auto res = systemsByStage.equal_range(event_id);
  for (auto sysIt = res.first; sysIt != res.second; ++sysIt)
    systems[sysIt->second].sys(ev, queries[systems[sysIt->second].queryId.index]);

  checkFrameSnapshot(snapshot);
}

void EntityManager::enableChangeDetection(const HashedString &name)
{
  if (name == HASH("eid"))
    return;
  DEBUG_LOG("enableChangeDetection: " << name.str);
  trackComponents.insert(name);
}

void EntityManager::disableChangeDetection(const HashedString &name)
{
  DEBUG_LOG("disableChangeDetection: " << name.str);
  trackComponents.erase(name);
}

QueryId EntityManager::createQuery(const ConstHashedString &name, const QueryDescription &desc, const filter_t &filter)
{
  QueryId qid = qidFactory.allocate();
  if (qid.index >= queries.size())
  {
    queries.resize(qid.index + 1);
    queryDescriptions.resize(qid.index + 1);
  }

  queries[qid.index].reset();

  queries[qid.index].id = qid;
  queries[qid.index].name = name;
  queryDescriptions[qid.index] = desc;
  queryDescriptions[qid.index].filter = filter;
  findArchetypes(queryDescriptions[qid.index]);
  return qid;
}

QueryId EntityManager::createQuery(const ConstHashedString &name, const ConstQueryDescription &desc, const filter_t &filter)
{
  return createQuery(name, QueryDescription(desc), filter);
}

void EntityManager::deleteQuery(const QueryId &qid)
{
  if (!qidFactory.isValid(qid))
    return;
  queries[qid.index].reset();
  queryDescriptions[qid.index].reset();
  qidFactory.free(qid);
}