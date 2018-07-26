#pragma once

#include "ecs/component.h"

struct TimerComponent
{
  float time = 0.f;
  float period = 5.f;

  bool set(const JValue &value)
  {
    assert(value.HasMember("time"));
    time = value["time"].GetFloat();
    assert(value.HasMember("period"));
    period = value["period"].GetFloat();
    return true;
  }
};
REG_COMP(TimerComponent, timer);

struct NoHitTimerComponent
{
  float time = 0.f;

  bool set(const JValue &value)
  {
    assert(value.HasMember("time"));
    time = value["time"].GetFloat();
    return true;
  }
};
REG_COMP(NoHitTimerComponent, no_hit_timer);