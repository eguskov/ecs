#include "ecs/ecs.h"

#include "update.h"

#include <random>
#include <ctime>
#include <future>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/default.component.h"
#include "components/timer.component.h"
#include "components/color.component.h"

extern int screen_width;
extern int screen_height;

REG_EVENT_INIT(EventOnSpawn);
REG_EVENT_INIT(EventOnAnotherTest);

struct Gravity
{
  bool set(const JValue&) { return true; };
};
REG_COMP_AND_INIT(Gravity, gravity);

//! @system
void update_position(
  const UpdateStage &stage,
  const glm::vec2 &vel,
  glm::vec2 &pos)
{
  pos += vel * stage.dt;
}
//REG_SYS_1(update_position, "vel", "pos");

//! @system
void update_vels(
  const UpdateStage &stage,
  const ArrayComp<glm::vec2, 2> &vels)
{
  vels[0];
}
//REG_SYS_1(update_vels, "vels");

//! @system
static inline void update_velocity(
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

  vel *= 1.0 - damping;
}
//REG_SYS_1(update_velocity, "damping", "pos", "pos_copy", "vel");

//! @system
//! @require(Gravity gravity)
static inline void update_collisions(
  const UpdateStage &stage,
  EntityId eid,
  float mass,
  const Gravity &gravity,
  const glm::vec2 &pos,
  glm::vec2 &vel)
{
  return;
  EntityId curEid = eid;
  glm::vec2 curPos = pos;
  glm::vec2 curVel = vel;
  float curMass = mass;

  g_mgr->query<void(EntityId, float, const glm::vec2&, glm::vec2&)>(
    [curEid, curPos, curMass, &stage](EntityId eid, float mass, const glm::vec2 &pos, glm::vec2 &v)
  {
    if (eid == curEid)
      return;
    static const float pix2m = 0.265f * 1e-3f;
    const float r = glm::length(curPos - pos) * pix2m;
    if (r <= 0.01f)
      return;
    const glm::vec2 f = glm::normalize(curPos - pos) * ((mass * curMass) / (r * r));
    v += f * stage.dt;
  },
  { "", "mass", "pos", "vel" });

  vel = curVel;
}
//REG_SYS_2(update_collisions, "mass", "gravity", "pos", "vel");

//! @system
void spawner(const UpdateStage &stage, EntityId eid, TimerComponent &timer)
{
  timer.time += stage.dt;
  if (timer.time >= timer.period)
  {
    timer.time = 0.f;
    // g_mgr->sendEvent(eid, EventOnSpawn{});
  }
}
//REG_SYS_2(spawner, "timer");

//! @system
void render(
  const RenderStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const glm::vec2 &pos)
{
  DrawCircleV(Vector2{ pos.x, pos.y }, 10, CLITERAL{ color.r, color.g, color.b, 255 });
}
//REG_SYS_2(render, "color", "pos");

//! @system
void spawn_handler(
  const EventOnSpawn &ev,
  EntityId eid,
  const glm::vec2 &vel,
  const glm::vec2 &pos)
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
//REG_SYS_2(spawn_handler, "vel", "pos");