#pragma once

#include "stdafx.h"

typedef uint32_t ecs_handle_t;

template <uint32_t GenerationBits, uint32_t IndexBits>
struct Handle
{
  static constexpr uint32_t INDEX_BITS  = IndexBits;
  static constexpr uint32_t INDEX_LIMIT = (1 << INDEX_BITS);
  static constexpr uint32_t INDEX_MASK  = (1 << INDEX_BITS) - 1;

  static constexpr uint32_t GENERATION_BITS  = GenerationBits;
  static constexpr uint32_t GENERATION_LIMIT = (1 << GENERATION_BITS);
  static constexpr uint32_t GENERATION_MASK  = (1 << GENERATION_BITS) - 1;

  union
  {
    struct
    {
      uint32_t index      : INDEX_BITS;
      uint32_t generation : GENERATION_BITS;
    };
    uint32_t handle = 0;
  };

  explicit Handle(ecs_handle_t h = 0) : handle(h) {}
  explicit Handle(uint32_t gen, uint32_t idx) : handle(((uint32_t)gen << INDEX_BITS | idx)) {}
  inline bool operator==(const Handle &rhs) const { return handle == rhs.handle; }
  inline bool operator!=(const Handle &rhs) const { return handle != rhs.handle; }
  inline operator bool() const { return handle != 0; }
};

using Handle_8_24 = Handle<8, 24>;

template <typename HandleType, uint32_t MinimumFreeIndices>
struct HandleFactory
{
  uint32_t handlesCount = 1;
  eastl::vector<int32_t> generations = {0};
  eastl::deque<int32_t>  freeIndexQueue;

  HandleType allocate()
  {
    if (freeIndexQueue.size() <= MinimumFreeIndices)
    {
      generations.emplace_back(0);
      return HandleType(0u, handlesCount++);
    }

    const int32_t freeIndex = freeIndexQueue.front();
    freeIndexQueue.pop_front();
    return HandleType(generations[freeIndex], freeIndex);
  }

  void free(const HandleType &h)
  {
    generations[h.index] = (h.generation + 1) % HandleType::GENERATION_LIMIT;
    freeIndexQueue.push_back(h.index);
  }

  inline bool isValid(const HandleType &h) const
  {
    return h.index <= generations.size() && h && h.generation == generations[h.index];
  }
};

struct EntityId : Handle_8_24
{
  EntityId(uint32_t h = 0) : Handle_8_24(h) {}
  explicit EntityId(uint32_t gen, uint32_t idx) : Handle_8_24(gen, idx) {}
};

struct QueryId : Handle_8_24
{
  QueryId(uint32_t h = 0) : Handle_8_24(h) {}
  explicit QueryId(uint32_t gen, uint32_t idx) : Handle_8_24(gen, idx) {}
};

using EntityVector = eastl::vector<EntityId>;

constexpr uint32_t kInvalidEid = 0;