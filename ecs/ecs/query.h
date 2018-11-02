#pragma once

#include "entity.h"

struct CompDesc
{
  int id;
  int nameId;
  const RegComp* desc;
};

struct QueryLink
{
  int firstTypeId = -1;
  int firstNameId = -1;

  int secondTypeId = -1;
  int secondNameId = -1;
};

struct QueryDesc
{
  eastl::vector<CompDesc> components;
  eastl::vector<CompDesc> haveComponents;
  eastl::vector<CompDesc> notHaveComponents;
  eastl::vector<CompDesc> isTrueComponents;
  eastl::vector<CompDesc> isFalseComponents;

  eastl::vector<int> roComponents;
  eastl::vector<int> rwComponents;

  eastl::vector<int> joinQueries;
  eastl::vector<QueryLink> joinLinks;

  bool isValid() const
  {
    for (const auto &c : components)
      if (c.nameId >= 0)
        return true;
    return
      !joinQueries.empty() ||
      !haveComponents.empty() ||
      !notHaveComponents.empty() ||
      !isTrueComponents.empty() ||
      !isFalseComponents.empty();
  }
};

struct Query
{
  bool dirty = false;

  int sysId = -1;
  int stageId = -1;
  QueryDesc desc;

  EntityVector eids;

#ifdef ECS_PACK_QUERY_DATA
  eastl::vector<int> data;
#endif
};