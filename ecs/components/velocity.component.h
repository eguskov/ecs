#pragma once

#include "ecs/component.h"

struct VelocityComponent
{
  float x = 3.f;
  float y = 4.f;
};
REG_COMP(VelocityComponent, velocity);