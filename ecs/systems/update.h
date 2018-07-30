#pragma once

#include "ecs/event.h"

struct EventOnSpawn : Event
{
};
REG_EVENT(EventOnSpawn);

struct EventOnAnotherTest : Event
{
  float x;
  float y;
  float z;
  EventOnAnotherTest(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};
REG_EVENT(EventOnAnotherTest);