#include "ecs/ecs.h"

#include "update.h"

#include <random>
#include <ctime>
#include <future>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/timer.component.h"
#include "components/color.component.h"

extern int screen_width;
extern int screen_height;

REG_EVENT_INIT(EventOnSpawn);
REG_EVENT_INIT(EventOnAnotherTest);

struct Gravity
{
  bool set(const JValue&) { return true; };
} DEF_COMP(Gravity, gravity);
// REG_COMP_AND_INIT(Gravity, gravity);

DEF_SYS()
static __forceinline void update_position(
  const UpdateStage &stage,
  const glm::vec2 &vel,
  glm::vec2 &pos)
{
  pos += vel * stage.dt;
}

DEF_SYS()
static __forceinline void update_vels(
  const UpdateStage &stage,
  const ArrayComp<glm::vec2, 2> &vels)
{
  vels[0];
}

DEF_SYS()
static __forceinline void update_velocity(
  const UpdateStage &stage,
  float damping,
  const glm::vec2 &pos,
  const glm::vec2 &pos_copy,
  glm::vec2 &vel)
{
  const int hw = 10;
  const int hh = 10;
  if (pos.x < hw || pos.x > screen_width - hw)
    vel.x = -vel.x;
  if (pos.y < hh || pos.y > screen_height - hh)
    vel.y = -vel.y;

  vel *= 1.0f / (1.0f + stage.dt * damping);
}

struct TestQuery DEF_QUERY();

DEF_SYS() HAS_COMP(gravity)
static __forceinline void update_collisions(
  const UpdateStage &stage,
  EntityId eid,
  float mass,
  const Gravity &gravity,
  glm::vec2 &pos,
  glm::vec2 &vel)
{
  EntityId curEid = eid;
  glm::vec2 curPos = pos;
  glm::vec2 curVel = vel;
  float curMass = mass;

  TestQuery::exec([curEid, curPos, &curVel, &stage](EntityId eid, float mass, const glm::vec2 &pos, const glm::vec2 &vel)
  {
    if (eid == curEid)
      return;
    const float r = glm::length(curPos - pos);
    if (r <= 0.01f || r > 10.f)
      return;
    const glm::vec2 f = (pos - curPos) * ((r - 10.f) / 10.f);
    curVel += f * stage.dt;
  });

  // vel = curVel;
}

DEF_SYS()
static __forceinline void spawner(const UpdateStage &stage, EntityId eid, TimerComponent &timer)
{
  timer.time += stage.dt;
  if (timer.time >= timer.period)
  {
    timer.time = 0.f;
    // g_mgr->sendEvent(eid, EventOnSpawn{});
  }
}

DEF_SYS()
static __forceinline void render(
  const RenderStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const glm::vec2 &pos)
{
  DrawCircleV(Vector2{ pos.x, pos.y }, 10, CLITERAL{ color.r, color.g, color.b, 255 });
}

DEF_SYS()
static __forceinline void spawn_handler(
  const EventOnSpawn &ev,
  EntityId eid,
  const glm::vec2 &vel,
  const glm::vec2 &pos)
{
  for (int i = 0; i < ev.count; ++i)
  {
    JDocument doc;
    auto &a = doc.GetAllocator();

    const float sx = (float)GetRandomValue(-50, 50);
    const float sy = (float)GetRandomValue(-50, 50);

    JValue posValue(rapidjson::kObjectType);
    posValue.AddMember("x", pos.x, a);
    posValue.AddMember("y", pos.y, a);

    JValue velValue(rapidjson::kObjectType);
    velValue.AddMember("x", sx, a);
    velValue.AddMember("y", sy, a);

    JValue colorValue(rapidjson::kObjectType);
    colorValue.AddMember("r", std::rand() % 256, a);
    colorValue.AddMember("g", std::rand() % 256, a);
    colorValue.AddMember("b", std::rand() % 256, a);

    JValue value(rapidjson::kObjectType);
    value.AddMember("pos", posValue, a);
    value.AddMember("vel", velValue, a);
    value.AddMember("color", colorValue, a);

    g_mgr->createEntity("ball", value);
  }
}