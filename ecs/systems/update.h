#pragma once

#include "ecs/event.h"

struct EventOnTest : Event
{
  float x;
  float y;
  EventOnTest(float _x, float _y) : x(_x), y(_y) {}
};
REG_EVENT(EventOnTest);