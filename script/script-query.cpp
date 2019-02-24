#include "script-query.h"

#include <ecs/ecs.h>
#include <ecs/debug.h>

#include "script.h"

namespace script
{

ScriptQuery::Iterator& ScriptQuery::Iterator::operator=(const ScriptQuery::Iterator &assign)
{
  chunkIdx = assign.chunkIdx;
  posInChunk = assign.posInChunk;
  sq = assign.sq;
  ASSERT(sq != nullptr);
  return *this;
}

bool ScriptQuery::Iterator::hasNext() const
{
  return
    chunkIdx < sq->query->chunksCount && 
    posInChunk < sq->query->entitiesInChunk[chunkIdx];
}

void ScriptQuery::Iterator::operator++()
{
  ++posInChunk;
  if (posInChunk >= sq->query->entitiesInChunk[chunkIdx])
  {
    posInChunk = 0;
    ++chunkIdx;
  }
}

void* ScriptQuery::Iterator::get()
{
  ASSERT(sq->type != nullptr);

  asIScriptFunction *fn = sq->type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  int compIdx = 0;
  for (const auto &c : sq->query->desc.components)
  {
    uint8_t *data = sq->query->chunks[compIdx + chunkIdx * sq->query->componentsCount].beginData;
    void *prop = object->GetAddressOfProperty(compIdx++);
    *(uint8_t **)prop = data + posInChunk * c.size;
  }

  return object;
}

ScriptQuery::Iterator ScriptQuery::perform()
{
  Iterator it;
  it.sq = this;
  return it;
}

asIScriptObject* inject_components_into_struct(const EntityId &eid, const eastl::vector<CompDesc> &components, asITypeInfo *type)
{
  // TODO: Reuse asIScriptObject *object;
  // TODO: Inject as SoA. Query result can be stored as Struct of Array in the script.
  asIScriptFunction *fn = type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  assert(eid.generation == g_mgr->entities[eid.index].eid.generation);

  const auto &entity = g_mgr->entities[eid.index];
  auto &archetype = g_mgr->archetypes[entity.archetypeId];

  int compIdx = 0;
  for (const auto &c : components)
  {
    void *prop = object->GetAddressOfProperty(compIdx++);
    *(uint8_t **)prop = archetype.storages[archetype.getComponentIndex(c.name)]->getRawByIndex(entity.indexInArchetype);
  }

  return object;
}

}