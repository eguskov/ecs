#pragma once

#include "ecs/ecs.h"

template <> struct Desc<EntityId> { constexpr static char* typeName = "EntityId"; constexpr static char* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  static int ID;

  bool init(uint8_t *, const JValue &) const override final { return true; }

  RegCompSpec() : RegComp("eid", sizeof(CompType))
  {
    ID = id;
  }
};