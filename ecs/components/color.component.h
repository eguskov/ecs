#pragma once

#include "ecs/component.h"

struct ColorComponent
{
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;

  bool set(const JValue &value)
  {
    assert(value.HasMember("r"));
    r = (uint8_t)value["r"].GetInt();
    assert(value.HasMember("g"));
    g = (uint8_t)value["g"].GetInt();
    assert(value.HasMember("b"));
    b = (uint8_t)value["b"].GetInt();
    return true;
  }
};
REG_COMP(ColorComponent, color);