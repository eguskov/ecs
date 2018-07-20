#pragma once

#include "ecs/component.h"

struct PositionComponent
{
  float x = 1.f;
  float y = 2.f;

  bool set(const JValue &value)
  {
    assert(value.HasMember("x"));
    x = value["x"].GetFloat();
    assert(value.HasMember("y"));
    y = value["y"].GetFloat();
    return true;
  }
};
REG_COMP(PositionComponent, position);