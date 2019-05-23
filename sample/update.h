#pragma once

#include <ecs/event.h>
#include <ecs/component.h>

#include <raylib.h>

struct EventOnKillEnemy
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnKillEnemy() = default;
  EventOnKillEnemy(const glm::vec2 &p) : pos(p) {}
};

ECS_EVENT(EventOnKillEnemy);

struct Timer
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
};

ECS_COMPONENT_TYPE(Timer);

struct NoHitTimer
{
  float time = 0.f;

  bool set(const JFrameValue &value)
  {
    ASSERT(value.HasMember("time"));
    time = value["time"].GetFloat();
    return true;
  }
};

ECS_COMPONENT_TYPE(NoHitTimer);

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
};

ECS_COMPONENT_TYPE_ALIAS(ColorComponent, color);

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
};

ECS_COMPONENT_TYPE(AutoMove);

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
};

ECS_COMPONENT_TYPE(Jump);

struct Gravity
{
  float mass = 0.f;

  bool set(const JFrameValue &value)
  {
    mass = value["mass"].GetFloat();
    return true;
  };
};

ECS_COMPONENT_TYPE(Gravity);

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
};

ECS_COMPONENT_TYPE(TextureAtlas);