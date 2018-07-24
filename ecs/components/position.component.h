#pragma once

#include "ecs/component.h"

struct PositionComponent
{
  Vector2 p;

  bool set(const JValue &value)
  {
    assert(value.HasMember("x"));
    p.x = value["x"].GetFloat();
    assert(value.HasMember("y"));
    p.y = value["y"].GetFloat();
    return true;
  }
};
REG_COMP(PositionComponent, position);