#pragma once

#include "ecs/component.h"

struct VelocityComponent
{
  float x = 3.f;
  float y = 4.f;
  bool set(const JValue &value)
  {
    assert(value.HasMember("x"));
    x = value["x"].GetFloat();
    assert(value.HasMember("y"));
    y = value["y"].GetFloat();
    return true;
  }
};
REG_COMP(VelocityComponent, velocity);
REG_COMP_ARR(VelocityComponent, velocity, 2);