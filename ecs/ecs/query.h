#pragma once

#include "entity.h"

struct CompDesc
{
  int id;
  int nameId;
  const RegComp* desc;
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

  bool isValid() const
  {
    for (const auto &c : components)
      if (c.nameId >= 0)
        return true;
    return
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