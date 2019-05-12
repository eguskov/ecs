#pragma once

#include "query.h"

struct ComponentDescription;

struct Index
{
  struct Item
  {
    int32_t queryId;
    uint32_t value;

    inline bool operator==(const Item &rhs) const { return value == rhs.value; }
    inline bool operator<(const Item &rhs) const { return value < rhs.value; }
  };

  bool dirty = false;

  HashedString name;
  HashedString componentName;

  QueryDescription desc;

  eastl::vector<Item> items;
  eastl::vector<Query> queries;

  Query *find(uint32_t value);
};

struct IndexDescription
{
  ConstHashedString name;
  ConstHashedString componentName;
  ConstQueryDescription desc;

  filter_t filter;

  const IndexDescription *next = nullptr;

  IndexDescription(const ConstHashedString &name, const ConstHashedString &component_name, const ConstQueryDescription &desc, filter_t &&f = nullptr);
};