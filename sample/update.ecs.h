#pragma once

#include <ecs/event.h>
#include <ecs/component.h>

#include <raylib.h>

struct EventOnSpawn : Event
{
  int count;
  EventOnSpawn(int _count) : count(_count) {}
}
DEF_EVENT(EventOnSpawn);

struct EventOnAnotherTest : Event
{
  float x;
  float y;
  float z;
  EventOnAnotherTest(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
}
DEF_EVENT(EventOnAnotherTest);

struct EventOnKillEnemy : Event
{
}
DEF_EVENT(EventOnKillEnemy);

struct EventOnWallHit : Event
{
  glm::vec2 normal = { 0.f, 0.f };
  EventOnWallHit() = default;
  EventOnWallHit(const glm::vec2 &n) : normal(n) {}
}
DEF_EVENT(EventOnWallHit);

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
}
DEF_COMP(TimerComponent, timer);

struct NoHitTimerComponent
{
  float time = 0.f;

  bool set(const JValue &value)
  {
    assert(value.HasMember("time"));
    time = value["time"].GetFloat();
    return true;
  }
}
DEF_COMP(NoHitTimerComponent, no_hit_timer);

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
}
DEF_COMP(ColorComponent, color);