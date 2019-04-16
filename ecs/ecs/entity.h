#pragma once

#include "stdafx.h"

struct EntityId
{
  static constexpr uint32_t INDEX_BITS = 24;
  static constexpr uint32_t INDEX_LIMIT = (1 << INDEX_BITS);
  static constexpr uint32_t INDEX_MASK = (1 << INDEX_BITS) - 1;

  static constexpr uint32_t GENERATION_BITS = 8;
  static constexpr uint32_t GENERATION_MASK = (1 << GENERATION_BITS) - 1;

  union
  {
    struct
    {
      uint32_t index : 24;
      uint32_t generation : 8;
    };
    uint32_t handle = 0xFFFFFFFF;
  };

  EntityId(uint32_t h = 0xFFFFFFFF) : handle(h) {}
  bool operator==(const EntityId &rhs) const { return handle == rhs.handle; }
  bool operator!=(const EntityId &rhs) const { return handle != rhs.handle; }
};

inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId(((uint32_t)gen << EntityId::INDEX_BITS | index));
}

inline int eid2idx(EntityId eid)
{
  return eid.handle & EntityId::INDEX_MASK;
}

using EntityVector = eastl::vector<EntityId>;

constexpr uint32_t kInvalidEid = 0xFFFFFFFF;