#pragma once

#include "entity.h"
#include "hash.h"

#include <EASTL/functional.h>

struct Archetype;

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

template <int N, int I = N - 1>
struct index_of_component
{
  static constexpr int get(const ConstHashedString &name, const ConstCompDesc (&components)[N])
  {
    return components[I].name.hash == name.hash ? I : index_of_component<N, I - 1>::get(name, components);
  }
};

template <int N>
struct index_of_component<N, -1>
{
  static constexpr int get(const ConstHashedString &name, const ConstCompDesc (&components)[N])
  {
    return -1;
  }
};

constexpr ConstQueryDesc empty_query_desc
{
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array
};

struct CompDesc
{
  int id;
  HashedString name;
  int size;
  const RegComp* desc;

  CompDesc& operator=(const ConstCompDesc &d)
  {
    id = -1;
    desc = nullptr;
    name = d.name;
    size = d.size;
    return *this;
  }
};

using filter_t = eastl::function<bool(const Archetype&, int)>;

struct RegQuery
{
  ConstHashedString name;
  ConstQueryDesc desc;

  filter_t filter;

  const RegQuery *next = nullptr;

  RegQuery(const ConstHashedString &name, const ConstQueryDesc &desc, filter_t &&f = nullptr);
};

struct QueryLink
{
  // TODO: Link by query name
  int firstTypeId = -1;
  HashedString firstComponentName;

  int secondTypeId = -1;
  HashedString secondComponentName;

  int componentSize = -1;
  const RegComp *componentDesc = nullptr;
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

  filter_t filter;

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

  int getComponentIndex(const HashedString &name) const
  {
    for (int i = 0; i < (int)components.size(); ++i)
      if (components[i].name == name)
        return i;
    return -1;
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
  uint32_t elemSize = 0;
};

// TODO: Query unittest
struct Query
{
  struct AllIterator
  {
    int idx = 0;
    int chunkIdx = 0;
    int componentsCount = 0;
    int chunksCount = 0;

    const int *entitiesInChunk = nullptr;
    QueryChunk *chunks = nullptr;

    AllIterator(QueryChunk *_chunks, int chunks_count, int *entities_in_chunk, int components_count, int chunk_idx = 0) :
      chunks(_chunks),
      entitiesInChunk(entities_in_chunk),
      componentsCount(components_count),
      chunksCount(chunks_count),
      chunkIdx(chunk_idx)
    {
    }

    bool operator==(const AllIterator &rhs) const
    {
      return idx == rhs.idx && chunkIdx == rhs.chunkIdx;
    }

    bool operator!=(const AllIterator &rhs) const
    {
      return !(operator==(rhs));
    }

    void operator++()
    {
      ++idx;
      if (idx >= entitiesInChunk[chunkIdx])
      {
        idx = 0;
        ++chunkIdx;
      }
    }

    uint8_t* getRaw(int comp_idx)
    {
      auto &chunk = chunks[comp_idx + chunkIdx * componentsCount];
      return chunk.beginData + idx * chunk.elemSize;
    }

    template<typename T>
    T& get(int comp_idx)
    {
      return *(T*)getRaw(comp_idx);
    }

    inline AllIterator operator*()
    {
      return *this;
    }

    inline AllIterator begin()
    {
      return *this;
    }

    inline AllIterator end()
    {
      return AllIterator(nullptr, -1, nullptr, -1, chunksCount);
    }
  };

  struct RawIterator
  {
    int idx = 0;
    int chunkIdx = 0;
    int compIdx = 0;
    int compSize = 0;
    int componentsCount = 0;
    int chunksCount = 0;

    const int *entitiesInChunk = nullptr;
    QueryChunk *chunks = nullptr;

    RawIterator(QueryChunk *_chunks, int chunks_count, int *entities_in_chunk, int components_count, int comp_idx, int comp_size, int chunk_idx = 0) :
      chunks(_chunks),
      entitiesInChunk(entities_in_chunk),
      componentsCount(components_count),
      chunksCount(chunks_count),
      compIdx(comp_idx),
      compSize(comp_size),
      chunkIdx(chunk_idx)
    {
    }

    bool operator==(const RawIterator &rhs) const
    {
      return idx == rhs.idx && chunkIdx == rhs.chunkIdx;
    }

    bool operator!=(const RawIterator &rhs) const
    {
      return !(operator==(rhs));
    }

    void operator++()
    {
      ++idx;
      if (idx >= entitiesInChunk[chunkIdx])
      {
        idx = 0;
        ++chunkIdx;
      }
    }

    uint8_t* operator*()
    {
      return chunks[compIdx + chunkIdx * componentsCount].beginData + idx * compSize;
    }

    template<typename T>
    T& get()
    {
      return *(T*)(chunks[compIdx + chunkIdx * componentsCount].beginData + idx * compSize);
    }

    inline RawIterator begin()
    {
      return *this;
    }

    inline RawIterator end()
    {
      return RawIterator(nullptr, -1, nullptr, -1, -1, -1, chunksCount);
    }
  };

  template <typename T>
  struct Iterator
  {
    using Self = Iterator<T>;

    int idx = 0;
    int chunkIdx = 0;
    int compIdx = 0;
    int compSize = 0;
    int componentsCount = 0;
    int chunksCount = 0;

    const int *entitiesInChunk = nullptr;
    QueryChunk *chunks = nullptr;

    Iterator(QueryChunk *_chunks, int chunks_count, int *entities_in_chunk, int components_count, int comp_idx, int comp_size, int chunk_idx = 0) :
      chunks(_chunks),
      entitiesInChunk(entities_in_chunk),
      componentsCount(components_count),
      chunksCount(chunks_count),
      compIdx(comp_idx),
      compSize(comp_size),
      chunkIdx(chunk_idx)
    {
    }

    bool operator==(const Iterator &rhs) const
    {
      return idx == rhs.idx && chunkIdx == rhs.chunkIdx;
    }

    bool operator!=(const Iterator &rhs) const
    {
      return !(operator==(rhs));
    }

    void operator++()
    {
      ++idx;
      if (idx >= entitiesInChunk[chunkIdx])
      {
        idx = 0;
        ++chunkIdx;
      }
    }

    T& operator*()
    {
      return *(T*)(chunks[compIdx + chunkIdx * componentsCount].beginData + idx * compSize);
    }

    inline Self begin()
    {
      return *this;
    }

    inline Self end()
    {
      return Self(nullptr, -1, nullptr, -1, -1, -1, chunksCount);
    }
  };

  inline RawIterator iter(int comp_idx)
  {
    ASSERT(comp_idx >= 0);
    return RawIterator(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, comp_idx, desc.components[comp_idx].size);
  }

  inline RawIterator iter(const HashedString &name)
  {
    const int compIdx = desc.getComponentIndex(name);
    ASSERT(compIdx >= 0);
    return RawIterator(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, compIdx, desc.components[compIdx].size);
  }

  inline RawIterator iter(const ConstHashedString &name)
  {
    const int compIdx = desc.getComponentIndex(name);
    ASSERT(compIdx >= 0);
    return RawIterator(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, compIdx, desc.components[compIdx].size);
  }

  template <typename T>
  inline Iterator<T> iter(const ConstHashedString &name)
  {
    const int compIdx = desc.getComponentIndex(name);
    ASSERT(compIdx >= 0);
    return Iterator<T>(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, compIdx, desc.components[compIdx].size);
  }

  template <>
  inline Iterator<EntityId> iter(const ConstHashedString &name)
  {
    const int compIdx = componentsCount - 1;
    return Iterator<EntityId>(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, compIdx, sizeof(EntityId));
  }

  template <typename T>
  inline Iterator<T> iter(int comp_idx)
  {
    ASSERT(comp_idx >= 0 && comp_idx < (int)desc.components.size());
    return Iterator<T>(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, comp_idx, desc.components[comp_idx].size);
  }

  template <>
  inline Iterator<EntityId> iter(int)
  {
    const int compIdx = componentsCount - 1;
    return Iterator<EntityId>(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount, compIdx, sizeof(EntityId));
  }

  inline AllIterator begin()
  {
    return AllIterator(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount);
  }

  inline AllIterator end()
  {
    return AllIterator(nullptr, -1, nullptr, -1, chunksCount);
  }

  bool dirty = false;

  int sysId = -1;
  int stageId = -1;

  HashedString name;
  // TODO: Do not add eid
  QueryDesc desc;

  int componentsCount = 0;
  int chunksCount = 0;
  int entitiesCount = 0;
  eastl::vector<int> entitiesInChunk;
  eastl::vector<QueryChunk> chunks;
};

struct JoinQuery : Query
{
  EntityVector eids;
};