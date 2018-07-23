#pragma once

#include "stdafx.h"

struct EntityId
{
  uint32_t handle = 0;
  EntityId(uint32_t h = 0) : handle(h) {}
  bool operator==(const EntityId &rhs) const { return handle == rhs.handle; }
};

inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId(((uint32_t)gen << 16 | index));
}

inline int eid2idx(EntityId eid)
{
  return eid.handle & 0xFFFF;
}