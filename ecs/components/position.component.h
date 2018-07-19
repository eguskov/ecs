#pragma once

#include "ecs/component.h"

struct PositionComponent
{
  float x = 1.f;
  float y = 2.f;
};
REG_COMP(PositionComponent, position);