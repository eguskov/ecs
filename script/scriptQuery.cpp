#include "scriptQuery.h"

#include <ecs/ecs.h>

#include <assert.h>

#include "script.h"

namespace script
{

ScriptQuery::Iterator& ScriptQuery::Iterator::operator=(const ScriptQuery::Iterator &assign)
{
  // TODO: Iterate by chunkIdx
  pos = assign.pos;
  sq = assign.sq;
  assert(sq != nullptr);
  return *this;
}

bool ScriptQuery::Iterator::hasNext() const
{
  return pos < (int)sq->query->entitiesCount;
}

void ScriptQuery::Iterator::operator++()
{
  ++pos;
}

void* ScriptQuery::Iterator::get()
{
  assert(sq->type != nullptr);

  asIScriptFunction *fn = sq->type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  // const EntityId &eid = sq->query->eids[pos];
  const auto &entity = g_mgr->entities[eid.index];
  // const auto &templ = g_mgr->templates[entity.templateId];
  auto &archetype = g_mgr->archetypes[entity.archetypeId];

  for (const auto &c : sq->query->desc.components)
  {
    bool ok = false;
    for (const auto &s : archetype.storages)
    {
      if (c.name == s.name)
      {
        ok = true;
        void *prop = object->GetAddressOfProperty(c.id);
        *(uint8_t **)prop = s.storage->getRawByIndex(entity.indexInArchetype);
        break;
      }
    }
    assert(ok);
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
  asIScriptFunction *fn = type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  const auto &entity = g_mgr->entities[eid.index];
  // const auto &templ = g_mgr->templates[entity.templateId];
  auto &archetype = g_mgr->archetypes[entity.archetypeId];

  for (const auto &c : components)
  {
    bool ok = false;
    for (const auto &s : archetype.storages)
    {
      if (c.name == s.name)
      {
        ok = true;
        void *prop = object->GetAddressOfProperty(c.id);
        *(uint8_t **)prop = s.storage->getRawByIndex(entity.indexInArchetype);
        break;
      }
    }
    assert(ok);
  }

  return object;
}

}