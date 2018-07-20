#pragma once

#include "ecs/event.h"

struct EventOnTest : Event
{
  float x;
  float y;
  EventOnTest(float _x, float _y) : x(_x), y(_y) {}
};
REG_EVENT(EventOnTest);

struct EventOnAnotherTest : Event
{
  float x;
  float y;
  float z;
  EventOnAnotherTest(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};
REG_EVENT(EventOnAnotherTest);