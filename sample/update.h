#pragma once

#include <ecs/event.h>
#include <ecs/component.h>

#include <raylib.h>

struct EventOnKillEnemy : Event
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnKillEnemy() = default;
  EventOnKillEnemy(const glm::vec2 &p) : pos(p) {}
}
DEF_EVENT(EventOnKillEnemy);

struct TimerComponent
{
  float time = 0.f;
  float period = 5.f;

  bool set(const JFrameValue &value)
  {
    ASSERT(value.HasMember("time"));
    time = value["time"].GetFloat();
    ASSERT(value.HasMember("period"));
    period = value["period"].GetFloat();
    return true;
  }
}
DEF_COMP(TimerComponent, timer);

struct NoHitTimerComponent
{
  float time = 0.f;

  bool set(const JFrameValue &value)
  {
    ASSERT(value.HasMember("time"));
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

  bool set(const JFrameValue &value)
  {
    ASSERT(value.HasMember("r"));
    r = (uint8_t)value["r"].GetInt();
    ASSERT(value.HasMember("g"));
    g = (uint8_t)value["g"].GetInt();
    ASSERT(value.HasMember("b"));
    b = (uint8_t)value["b"].GetInt();
    return true;
  }
}
DEF_COMP(ColorComponent, color);

struct AutoMove
{
  bool jump = false;

  float time = 0.f;
  float duration = 0.f;
  float length = 0.f;

  bool set(const JFrameValue &value)
  {
    jump = value["jump"].GetBool();
    duration = value["duration"].GetFloat();
    length = value["length"].GetFloat();
    time = duration;
    return true;
  }
}
DEF_COMP(AutoMove, auto_move);

struct Jump
{
  bool active = false;

  double startTime = 0.0;
  float height = 0.f;
  float duration = 0.f;

  bool set(const JFrameValue &value)
  {
    height = value["height"].GetFloat();
    duration = value["duration"].GetFloat();
    return true;
  };
}
DEF_COMP(Jump, jump);

struct Gravity
{
  float mass = 0.f;

  bool set(const JFrameValue &value)
  {
    mass = value["mass"].GetFloat();
    return true;
  };
}
DEF_COMP(Gravity, gravity);

struct TextureAtlas
{
  eastl::string path;
  Texture2D id;

  TextureAtlas() = default;
  TextureAtlas(TextureAtlas &&assign);

  void operator=(TextureAtlas &&assign);

  TextureAtlas(const TextureAtlas&) { ASSERT(false); }
  void operator=(const TextureAtlas&) { ASSERT(false); }

  bool set(const JFrameValue &value)
  {
    ASSERT(value.HasMember("path"));
    path = value["path"].GetString();
    return true;
  };
}
DEF_COMP(TextureAtlas, texture);