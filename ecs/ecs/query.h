#pragma once

#include "entity.h"
#include "hash.h"

struct CompDesc
{
  int id;
  int nameId;
  HashedString name;
  int size;
  const RegComp* desc;
};

struct QueryLink
{
  int firstTypeId = -1;
  HashedString firstName;

  int secondTypeId = -1;
  HashedString secondName;
};

// TODO: Use constexpr QueryDescs in codegen
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

  int getComponentIndex(const ConstHashedString &name) const
  {
    for (int i = 0; i < (int)components.size(); ++i)
      if (components[i].name == name)
        return i;
    return -1;
  }

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

struct QueryChunk
{
  template <typename T>
  struct Iterator
  {
    using Self = Iterator<T>;

    uint8_t *cur;
    uint8_t *end;

    Iterator(uint8_t *b, uint8_t *e) : cur(b), end(e) {}
    Iterator(const Self& assign) : cur(assign.cur), end(assign.end) {}

    Self& operator=(Self&& assign)
    {
      cur = assign.cur;
      end = assign.end;
      return *this;
    }

    Self& operator=(const Self& assign)
    {
      cur = assign.cur;
      end = assign.end;
      return *this;
    }

    T& operator*()
    {
      return *(T*)cur;
    }

    T* operator->()
    {
      return (T*)cur;
    }

    bool operator==(const Self& rhs) const
    {
      return cur == rhs.cur;
    }

    void operator++()
    {
      cur += sizeof(T);
    }
  };

  template <typename T>
  Iterator<T> begin()
  {
    return Iterator<T>(beginData, endData);
  }

  template <typename T>
  Iterator<T> end()
  {
    return Iterator<T>(endData, endData);
  }

  uint8_t *beginData = nullptr;
  uint8_t *endData = nullptr;
};

struct Query
{
  bool dirty = false;

  int sysId = -1;
  int stageId = -1;

  QueryDesc desc;

  // EntityVector eids;

  int componentsCount = 0;
  int chunksCount = 0;
  int entitiesCount = 0;
  eastl::vector<int> entitiesInChunk;
  eastl::vector<QueryChunk> chunks;
};