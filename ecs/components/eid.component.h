#pragma once

#include "ecs/ecs.h"

extern int reg_comp_count;

template <> struct Desc<EntityId> { constexpr static char* typeName = "EntityId"; constexpr static char* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  void init(uint8_t *) const override final {}

  RegCompSpec() : RegComp("eid", reg_comp_count, sizeof(CompType))
  {
  }
};