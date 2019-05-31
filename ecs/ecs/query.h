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

enum ComponentDescriptionFlags
{
  kNone = 0,
  kWrite = 1 << 0,
};

struct ConstComponentDescription
{
  ConstHashedString name;
  uint32_t size;
  uint32_t flags = ComponentDescriptionFlags::kNone;
};

constexpr ConstArray<const ConstComponentDescription> empty_desc_array = make_empty_array<const ConstComponentDescription>();

struct ConstQueryDescription
{
  ConstArray<const ConstComponentDescription> components;
  ConstArray<const ConstComponentDescription> haveComponents;
  ConstArray<const ConstComponentDescription> notHaveComponents;
  ConstArray<const ConstComponentDescription> trackComponents;
};

template <int N, int I = N - 1>
struct index_of_component
{
  static constexpr int get(const ConstHashedString &name, const ConstComponentDescription (&components)[N])
  {
    return components[I].name.hash == name.hash ? I : index_of_component<N, I - 1>::get(name, components);
  }
};

template <int N>
struct index_of_component<N, -1>
{
  static constexpr int get(const ConstHashedString &name, const ConstComponentDescription (&components)[N])
  {
    return -1;
  }
};

constexpr ConstQueryDescription empty_query_desc
{
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array
};

struct Component
{
  int id;
  HashedString name;
  uint32_t size;
  const ComponentDescription* desc;

  Component& operator=(const ConstComponentDescription &d)
  {
    id = -1;
    desc = nullptr;
    name = d.name;
    size = d.size;
    return *this;
  }
};

using filter_t = eastl::function<bool(const Archetype&, int)>;

struct PersistentQueryDescription
{
  ConstHashedString name;
  ConstQueryDescription desc;

  filter_t filter;

  static const PersistentQueryDescription *head;
  static int count;

  const PersistentQueryDescription *next = nullptr;

  PersistentQueryDescription(const ConstHashedString &name, const ConstQueryDescription &desc, filter_t &&f = nullptr);
};

// TODO: Use constexpr QueryDescs in codegen
struct QueryDescription
{
  eastl::vector<Component> components;

  eastl::vector<Component> haveComponents;
  eastl::vector<Component> notHaveComponents;

  eastl::vector<int> archetypes;

  filter_t filter;

  QueryDescription& operator=(const ConstQueryDescription &desc)
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

  bool isDependOnComponent(const HashedString &name) const
  {
    for (int i = 0; i < (int)components.size(); ++i)
      if (components[i].name == name)
        return true;
    for (int i = 0; i < (int)haveComponents.size(); ++i)
      if (haveComponents[i].name == name)
        return true;
    for (int i = 0; i < (int)notHaveComponents.size(); ++i)
      if (notHaveComponents[i].name == name)
        return true;
    return false;
  }

  bool isDependOnComponent(const ConstHashedString &name) const
  {
    for (int i = 0; i < (int)components.size(); ++i)
      if (components[i].name == name)
        return true;
    for (int i = 0; i < (int)haveComponents.size(); ++i)
      if (haveComponents[i].name == name)
        return true;
    for (int i = 0; i < (int)notHaveComponents.size(); ++i)
      if (notHaveComponents[i].name == name)
        return true;
    return false;
  }

  bool isValid() const
  {
    return
      !components.empty() ||
      !haveComponents.empty() ||
      !notHaveComponents.empty();
  }
};

// TODO: Query unittest
struct Query
{
  struct ChunkIterator
  {
    uint8_t * __restrict * __restrict chunks;
    const int * __restrict entitiesInChunk;
    int componentsCount;
    int offset;

    ChunkIterator(uint8_t * __restrict * __restrict _chunks, const int * __restrict entities_in_chunk, int components_count, int _offset = 0):
      chunks(_chunks),
      entitiesInChunk(entities_in_chunk),
      componentsCount(components_count),
      offset(_offset)
    {}

    inline bool operator==(const ChunkIterator &rhs) const
    {
      return chunks == rhs.chunks;
    }

    inline bool operator!=(const ChunkIterator &rhs) const
    {
      return !(operator==(rhs));
    }

    inline void operator++()
    {
      chunks += componentsCount;
      ++entitiesInChunk;
      offset = 0;
    }

    inline uint8_t * __restrict * __restrict operator*()
    {
      return chunks;
    }

    inline int32_t begin() const
    {
      return offset;
    }

    inline int32_t end() const
    {
      return *entitiesInChunk;
    }

    template<typename T>
    inline T& __restrict get(int32_t component_index, int32_t index_in_chunk)
    {
      return *(((T * __restrict)chunks[component_index]) + index_in_chunk);
    }
  };
  
  struct AllIterator
  {
    int idx = 0;
    int chunkIdx = 0;
    int componentsCount = 0;
    int chunksCount = 0;

    const int * __restrict entitiesInChunk = nullptr;
    uint8_t * __restrict * __restrict chunks = nullptr;
    uint8_t * __restrict * __restrict curChunk = nullptr;

    AllIterator(uint8_t * __restrict * __restrict _chunks, int chunks_count, int * __restrict entities_in_chunk, int components_count, int chunk_idx = 0) :
      chunks(_chunks),
      entitiesInChunk(entities_in_chunk),
      componentsCount(components_count),
      chunksCount(chunks_count),
      chunkIdx(chunk_idx)
    {
      curChunk = chunks;
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
        curChunk += componentsCount;
      }
    }

    template<typename T>
    inline T& get(int comp_idx)
    {
      uint8_t **chunk = curChunk + comp_idx;
      return ((T*)*chunk)[idx];
    }

    template<typename T>
    inline T& get(int comp_idx) const
    {
      uint8_t **chunk = curChunk + comp_idx;
      return ((T*)*chunk)[idx];
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

    inline void advance(int offset)
    {
      idx += offset;
      while (idx >= entitiesInChunk[chunkIdx] && chunkIdx < chunksCount) {
        idx -= entitiesInChunk[chunkIdx];
        ++chunkIdx;
      }
    }

    inline AllIterator operator+(int offset) const
    {
      AllIterator it(*this);
      it.advance(offset);
      return it;
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
    uint8_t **chunks = nullptr;

    RawIterator(uint8_t **_chunks, int chunks_count, int *entities_in_chunk, int components_count, int comp_idx, int comp_size, int chunk_idx = 0) :
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
      return chunks[compIdx + chunkIdx * componentsCount] + idx * compSize;
    }

    template<typename T>
    T& get()
    {
      return *(T*)(chunks[compIdx + chunkIdx * componentsCount] + idx * compSize);
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
    uint8_t **chunks = nullptr;

    Iterator(uint8_t **_chunks, int chunks_count, int *entities_in_chunk, int components_count, int comp_idx, int comp_size, int chunk_idx = 0) :
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
      return *(T*)(chunks[compIdx + chunkIdx * componentsCount] + idx * compSize);
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

  inline AllIterator begin(int offset)
  {
    auto iter = AllIterator(chunks.data(), chunksCount, entitiesInChunk.data(), componentsCount);
    iter.advance(offset);
    return iter;
  }

  inline ChunkIterator beginChunk(int offset)
  {
    uint8_t * __restrict * __restrict chunksData = chunks.data();
    int * __restrict entitiesInChunkData = entitiesInChunk.data();
    int * __restrict entitiesInChunkDataEnd = entitiesInChunk.data() + chunksCount;
    while (offset >= *entitiesInChunkData && entitiesInChunkData != entitiesInChunkDataEnd) {
      offset -= *entitiesInChunkData;
      ++entitiesInChunkData;
      chunksData += componentsCount;
    }
    return ChunkIterator(chunksData, entitiesInChunkData, componentsCount, offset);
  }

  inline ChunkIterator beginChunk()
  {
    return ChunkIterator(chunks.data(), entitiesInChunk.data(), componentsCount);
  }

  inline ChunkIterator endChunk()
  {
    return ChunkIterator(chunks.data() + chunksCount * componentsCount, entitiesInChunk.data(), componentsCount);
  }

  void addChunks(const QueryDescription &in_desc, Archetype &type, int begin, int entities_count);

  HashedString name;

  QueryDescription desc;

  int componentsCount = 0;
  int chunksCount = 0;
  int entitiesCount = 0;
  eastl::vector<int> entitiesInChunk;
  eastl::vector<uint8_t * __restrict> chunks;
};

template <typename T, int I>
struct StructField
{
  using Type = T;
  static constexpr int index = I;
};

template <typename Head, typename... Tail>
struct StructBuilder
{
  template <typename T>
  struct helper
  {
    static inline typename T::Type& get(const Query::AllIterator &iter)
    {
      return iter.get<typename T::Type>(T::index);
    }
  };

  template <typename Struct>
  static inline Struct build(const Query::AllIterator &iter)
  {
    return { helper<Head>::get(iter), helper<Tail>::get(iter)... };
  }
};

template <typename T, typename Builder>
struct QueryIterable
{
  Query::AllIterator first;
  Query::AllIterator last;

  int entitiesCount = -1;

  QueryIterable(Query &query) : first(query.begin()), last(query.end()), entitiesCount(query.entitiesCount)
  {
  }

  QueryIterable(Query::AllIterator _first, Query::AllIterator _last) : first(_first), last(_last)
  {
  }

  inline int count()
  {
    return entitiesCount;
  }

  inline QueryIterable begin()
  {
    return QueryIterable(first, last);
  }

  inline QueryIterable end()
  {
    return QueryIterable(last, last);
  }

  static inline T get(const Query::AllIterator &it)
  {
    return typename Builder::template build<T>(it);
  }

  inline T operator*()
  {
    return get(first);
  }

  inline void operator++()
  {
    ++first;
  }

  inline bool operator!=(const QueryIterable &rhs)
  {
    return first != rhs.first;
  }
};