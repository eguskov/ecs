#pragma once

#include "entity.h"
#include "hash.h"

template <typename T>
struct ConstArray
{
  const T *data;
  int sz;
  constexpr ConstArray(const T *d, int s) : data(d), sz(s) {}

  int size() const { return sz; }

  const T* begin() const { return data; }
  const T* end() const { return data + sz; }

  const T& operator[](int i) const { return data[i]; }
};

template <typename T, size_t N>
constexpr ConstArray<T> make_const_array(T (&array)[N])
{
  return ConstArray<T>(array, N);
}

template <typename T>
static constexpr ConstArray<T> make_empty_array()
{
  return ConstArray<T>(nullptr, 0);
}

struct ConstCompDesc
{
  ConstHashedString name;
  int size;
};

constexpr ConstArray<const ConstCompDesc> empty_desc_array = make_empty_array<const ConstCompDesc>();

struct ConstQueryDesc
{
  ConstArray<const ConstCompDesc> components;
  ConstArray<const ConstCompDesc> haveComponents;
  ConstArray<const ConstCompDesc> notHaveComponents;
  ConstArray<const ConstCompDesc> isTrueComponents;
  ConstArray<const ConstCompDesc> isFalseComponents;
};

struct CompDesc
{
  int id;
  int nameId;
  HashedString name;
  int size;
  const RegComp* desc;

  CompDesc& operator=(const ConstCompDesc &d)
  {
    id = -1;
    nameId = -1;
    desc = nullptr;
    name = d.name;
    size = d.size;
    return *this;
  }
};

struct RegQuery
{
  ConstHashedString name;
  ConstQueryDesc desc;

  const RegQuery *next = nullptr;

  RegQuery(const ConstHashedString &name, const ConstQueryDesc &desc);
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

  QueryDesc& operator=(const ConstQueryDesc &desc)
  {
    components.resize(desc.components.size());
    for (int i = 0; i < desc.components.size(); ++i)
      components[i] = desc.components.data[i];
    haveComponents.resize(desc.haveComponents.size());
    for (int i = 0; i < desc.haveComponents.size(); ++i)
      haveComponents[i] = desc.haveComponents.data[i];
    notHaveComponents.resize(desc.notHaveComponents.size());
    for (int i = 0; i < desc.notHaveComponents.size(); ++i)
      notHaveComponents[i] = desc.notHaveComponents.data[i];
    isTrueComponents.resize(desc.isTrueComponents.size());
    for (int i = 0; i < desc.isTrueComponents.size(); ++i)
      isTrueComponents[i] = desc.isTrueComponents.data[i];
    isFalseComponents.resize(desc.isFalseComponents.size());
    for (int i = 0; i < desc.isFalseComponents.size(); ++i)
      isFalseComponents[i] = desc.isFalseComponents.data[i];
    return *this;
  }

  int getComponentIndex(const ConstHashedString &name) const
  {
    for (int i = 0; i < (int)components.size(); ++i)
      if (components[i].name == name)
        return i;
    return -1;
  }

  bool isValid() const
  {
    return
      !components.empty() ||
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

  HashedString name;
  QueryDesc desc;

  // TODO: This might be needed for Join queries
  // EntityVector eids;

  int componentsCount = 0;
  int chunksCount = 0;
  int entitiesCount = 0;
  eastl::vector<int> entitiesInChunk;
  eastl::vector<QueryChunk> chunks;
};