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

void update_position(
  const UpdateStage &stage,
  const glm::vec2 &vel,
  glm::vec2 &pos)
{
  pos += vel * stage.dt;
}
REG_SYS_1(update_position, "vel", "pos");

void update_vels(
  const UpdateStage &stage,
  const ArrayComp<glm::vec2, 2> &vels)
{
  vels[0];
}
REG_SYS_1(update_vels, "vels");

static inline void update_velocity(
  const UpdateStage &stage,
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
}
REG_SYS_1(update_velocity, "pos", "pos_copy", "vel");

//void update_collisions(
//  const UpdateStage &stage,
//  const CompArrayDesc &desc,
//  const PositionComponent *pos,
//  VelocityComponent *vel)
//{
//  for (int i = 0; i < desc.count; ++i)
//  {
//    auto &posi = pos[desc.offset(0)];
//  }
//}

static inline void update_collisions(
  const UpdateStage &stage,
  EntityId eid,
  const glm::vec2 &pos,
  glm::vec2 &vel)
{
  return;

  EntityId curEid = eid;
  glm::vec2 curPos = pos;
  glm::vec2 curVel = vel;

  g_mgr->query<void(EntityId, const glm::vec2&, const glm::vec2&)>(
    [curEid, &curPos, &curVel, &stage](EntityId eid, const glm::vec2 &pos, const glm::vec2 &vel)
  {
    if (eid == curEid)
      return;
    const float r = glm::length(curPos - pos);
    if (r <= 1.e-5f)
      return;
    const float m1 = 10.f;
    const float m2 = 10.f;
    const glm::vec2 f = glm::normalize(pos - curPos) * ((m1 * m2) / (r * r));
    curVel += f * stage.dt;
  },
  { "", "pos", "vel" });

  vel = curVel;
}
REG_SYS_2(update_collisions, "pos", "vel");

void spawner(const UpdateStage &stage, EntityId eid, TimerComponent &timer)
{
  timer.time += stage.dt;
  if (timer.time >= timer.period)
  {
    timer.time = 0.f;
    // g_mgr->sendEvent(eid, EventOnSpawn{});
  }
}
REG_SYS_2(spawner, "timer");

void render(
  const RenderStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const glm::vec2 &pos)
{
  DrawCircleV(Vector2{ pos.x, pos.y }, 10, CLITERAL{ color.r, color.g, color.b, 255 });
}
REG_SYS_2(render, "color", "pos");

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
REG_SYS_2(spawn_handler, "vel", "pos");