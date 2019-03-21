#pragma once

#include "stdafx.h"

struct EntityId
{
  union 
  {
    struct
    {
      uint32_t index : 16;
      uint32_t generation : 16;
    };
    uint32_t handle = 0xFFFFFFFF;
  };

  EntityId(uint32_t h = 0xFFFFFFFF) : handle(h) {}
  bool operator==(const EntityId &rhs) const { return handle == rhs.handle; }
  bool operator!=(const EntityId &rhs) const { return handle != rhs.handle; }
};

inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId(((uint32_t)gen << 16 | index));
}

inline int eid2idx(EntityId eid)
{
  return eid.handle & 0xFFFF;
}

using EntityVector = eastl::vector<EntityId>;

constexpr uint32_t kInvalidEid = 0xFFFFFFFF;