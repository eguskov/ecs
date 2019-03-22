#include <ecs/event.h>

struct EventOnClickMouseLeftButton : Event
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickMouseLeftButton() = default;
  EventOnClickMouseLeftButton(const glm::vec2 &p) : pos(p) {}
}
DEF_EVENT(EventOnClickMouseLeftButton);

struct EventOnClickSpace : Event
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickSpace() = default;
  EventOnClickSpace(const glm::vec2 &p) : pos(p) {}
}
DEF_EVENT(EventOnClickSpace);

struct EventOnClickLeftControl : Event
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnClickLeftControl() = default;
  EventOnClickLeftControl(const glm::vec2 &p) : pos(p) {}
}
DEF_EVENT(EventOnClickLeftControl);

struct EventOnChangeCohesion : Event
{
  float delta = 0.f;
  EventOnChangeCohesion() = default;
  EventOnChangeCohesion(const float &d) : delta(d) {}
}
DEF_EVENT(EventOnChangeCohesion);

struct EventOnChangeAlignment : Event
{
  float delta = 0.f;
  EventOnChangeAlignment() = default;
  EventOnChangeAlignment(const float &d) : delta(d) {}
}
DEF_EVENT(EventOnChangeAlignment);

struct EventOnChangeWander : Event
{
  float delta = 0.f;
  EventOnChangeWander() = default;
  EventOnChangeWander(const float &d) : delta(d) {}
}
DEF_EVENT(EventOnChangeWander);