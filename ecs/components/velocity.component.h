#pragma once

#include "ecs/component.h"

struct VelocityComponent
{
  Vector2 v;

  bool set(const JValue &value)
  {
    assert(value.HasMember("x"));
    v.x = value["x"].GetFloat();
    assert(value.HasMember("y"));
    v.y = value["y"].GetFloat();
    return true;
  }
};
REG_COMP(VelocityComponent, velocity);
REG_COMP_ARR(VelocityComponent, velocity, 2);