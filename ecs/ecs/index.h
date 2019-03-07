#pragma once

#include "query.h"

struct RegComp;

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

  QueryDesc desc;

  eastl::vector<Item> items;
  eastl::vector<Query> queries;

  Query *find(uint32_t value);
};

struct RegIndex
{
  ConstHashedString name;
  ConstHashedString componentName;
  ConstQueryDesc desc;

  const RegIndex *next = nullptr;

  RegIndex(const ConstHashedString &name, const ConstHashedString &component_name, const ConstQueryDesc &desc);
};