#include "scriptQuery.h"

#include <ecs/ecs.h>

#include <assert.h>

#include "script.h"

namespace script
{

ScriptQuery::Iterator& ScriptQuery::Iterator::operator=(const ScriptQuery::Iterator &assign)
{
  pos = assign.pos;
  sq = assign.sq;
  assert(sq != nullptr);
  return *this;
}

bool ScriptQuery::Iterator::hasNext() const
{
  return pos < (int)sq->query->eids.size();
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

  const EntityId &eid = sq->query->eids[pos];
  const auto &entity = g_mgr->entities[eid.index];
  const auto &templ = g_mgr->templates[entity.templateId];

  for (const auto &c : sq->query->desc.components)
  {
    if (c.desc->id == RegCompSpec<EntityId>::ID)
    {
      void *prop = object->GetAddressOfProperty(c.id);
      *(uint8_t **)prop = (uint8_t *)&eid;
      continue;
    }

    bool ok = false;
    for (const auto &tc : templ.components)
    {
      if (c.nameId == tc.nameId)
      {
        ok = true;
        void *prop = object->GetAddressOfProperty(c.id);
        *(uint8_t **)prop = g_mgr->storages[tc.nameId]->getRaw(entity.componentOffsets[tc.id]);
        break;
      }
    }
    assert(ok);
  }

  return object;
}

ScriptQuery::IteratorWithFilter::IteratorWithFilter(const ScriptQuery::IteratorWithFilter &assign)
{
  *this = assign;
}

ScriptQuery::IteratorWithFilter& ScriptQuery::IteratorWithFilter::operator=(const ScriptQuery::IteratorWithFilter &assign)
{
  pos = assign.pos;
  sq = assign.sq;
  assert(sq != nullptr);
  JFrameAllocator allocator;
  filter.CopyFrom(assign.filter, allocator);
  return *this;
}

bool ScriptQuery::IteratorWithFilter::hasNext() const
{
  return pos < (int)sq->query->eids.size();
}

void ScriptQuery::IteratorWithFilter::operator++()
{
  // TODO: FILTER !!!
  ++pos;
}

void* ScriptQuery::IteratorWithFilter::get()
{
  assert(sq->type != nullptr);

  asIScriptFunction *fn = sq->type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  const EntityId &eid = sq->query->eids[pos];
  const auto &entity = g_mgr->entities[eid.index];
  const auto &templ = g_mgr->templates[entity.templateId];

  for (const auto &c : sq->query->desc.components)
  {
    if (c.desc->id == RegCompSpec<EntityId>::ID)
    {
      void *prop = object->GetAddressOfProperty(c.id);
      *(uint8_t **)prop = (uint8_t *)&eid;
      continue;
    }

    bool ok = false;
    for (const auto &tc : templ.components)
    {
      if (c.nameId == tc.nameId)
      {
        ok = true;
        void *prop = object->GetAddressOfProperty(c.id);
        *(uint8_t **)prop = g_mgr->storages[tc.nameId]->getRaw(entity.componentOffsets[tc.id]);
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

ScriptQuery::IteratorWithFilter ScriptQuery::perform(const JFrameValue &m)
{
  JFrameAllocator allocator;
  IteratorWithFilter it;
  it.sq = this;
  it.filter.CopyFrom(m, allocator);
  return it;
}

asIScriptObject* inject_components_into_struct(const EntityId &eid, const eastl::vector<CompDesc> &components, asITypeInfo *type)
{
  asIScriptFunction *fn = type->GetFactoryByIndex(0);
  asIScriptObject *object = nullptr;
  callValue<asIScriptObject*>([&](asIScriptObject **obj) { object = *obj; object->AddRef(); }, fn);

  const auto &entity = g_mgr->entities[eid.index];
  const auto &templ = g_mgr->templates[entity.templateId];

  for (const auto &c : components)
  {
    if (c.desc->id == RegCompSpec<EntityId>::ID)
    {
      void *prop = object->GetAddressOfProperty(c.id);
      *(uint8_t **)prop = (uint8_t *)&eid;
      continue;
    }

    bool ok = false;
    for (const auto &tc : templ.components)
    {
      if (c.nameId == tc.nameId)
      {
        ok = true;
        void *prop = object->GetAddressOfProperty(c.id);
        *(uint8_t **)prop = g_mgr->storages[tc.nameId]->getRaw(entity.componentOffsets[tc.id]);
        break;
      }
    }
    assert(ok);
  }

  return object;
}

}