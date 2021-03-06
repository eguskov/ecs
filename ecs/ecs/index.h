#pragma once

#include "query.h"

struct ComponentDescription;

struct Index
{
  HashedString name;
  HashedString componentName;

  QueryDescription desc;

  eastl::hash_map<uint32_t, int> itemsMap;
  eastl::vector<Query> queries;

  Query *find(uint32_t value);
};

struct IndexDescription
{
  ConstHashedString name;
  ConstHashedString componentName;
  ConstQueryDescription desc;

  filter_t filter;

  static const IndexDescription *head;
  static int count;

  const IndexDescription *next = nullptr;

  IndexDescription(const ConstHashedString &name, const ConstHashedString &component_name, const ConstQueryDescription &desc, filter_t &&f = nullptr);
};