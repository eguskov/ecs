#include <ecs/event.h>

#include <glm/vec2.hpp>

#include <daScript/daScript.h>

struct EventOnClickMouseLeftButton
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickMouseLeftButton() = default;
  EventOnClickMouseLeftButton(const glm::vec2 &p) : pos(p) {}
};

ECS_EVENT(EventOnClickMouseLeftButton);

struct EventOnClickSpace
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickSpace() = default;
  EventOnClickSpace(const glm::vec2 &p) : pos(p) {}
};

ECS_EVENT(EventOnClickSpace);

struct EventOnClickLeftControl
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickLeftControl() = default;
  EventOnClickLeftControl(const glm::vec2 &p) : pos(p) {}
};

ECS_EVENT(EventOnClickLeftControl);

struct EventOnChangeCohesion
{
  float delta = 0.f;
  EventOnChangeCohesion() = default;
  EventOnChangeCohesion(const float &d) : delta(d) {}
};

ECS_EVENT(EventOnChangeCohesion);

struct EventOnChangeAlignment
{
  float delta = 0.f;
  EventOnChangeAlignment() = default;
  EventOnChangeAlignment(const float &d) : delta(d) {}
};

ECS_EVENT(EventOnChangeAlignment);

struct EventOnChangeSeparation
{
  float delta = 0.f;
  EventOnChangeSeparation() = default;
  EventOnChangeSeparation(const float &d) : delta(d) {}
};

ECS_EVENT(EventOnChangeSeparation);

struct EventOnChangeWander
{
  float delta = 0.f;
  EventOnChangeWander() = default;
  EventOnChangeWander(const float &d) : delta(d) {}
};

ECS_EVENT(EventOnChangeWander);