#pragma once

#include "ecs/ecs.h"

template <> struct Desc<EntityId> { constexpr static char* typeName = "EntityId"; constexpr static char* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  void init(uint8_t *) const override final {}

  RegCompSpec() : RegComp("eid", sizeof(CompType))
  {
  }
};