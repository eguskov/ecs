#include "ecs/ecs.h"

#include "update.h"

#include <random>
#include <ctime>
#include <future>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/position.component.h"
#include "components/velocity.component.h"
#include "components/timer.component.h"
#include "components/color.component.h"

extern int screen_width;
extern int screen_height;

REG_EVENT_INIT(EventOnSpawn);
REG_EVENT_INIT(EventOnAnotherTest);

void update_position(
  const UpdateStage &stage,
  const VelocityComponent &vel,
  PositionComponent &pos)
{
  pos.p = Vector2Add(pos.p, Vector2Scale(vel.v, stage.dt));
}
REG_SYS_1(update_position, "vel", "pos");

void update_vels(
  const UpdateStage &stage,
  const ArrayComp<VelocityComponent, 2> &vels)
{
  vels[0].v;
}
REG_SYS_1(update_vels, "vels");

static inline void update_velocity(
  const UpdateStage &stage,
  const PositionComponent &pos,
  const PositionComponent &pos_copy,
  VelocityComponent &vel)
{
  const int hw = 10;
  const int hh = 10;
  if (pos.p.x < hw || pos.p.x > screen_width - hw)
    vel.v.x = -vel.v.x;
  if (pos.p.y < hh || pos.p.y > screen_height - hh)
    vel.v.y = -vel.v.y;
}
REG_SYS_1(update_velocity, "pos", "pos_copy", "vel");

void spawner(const UpdateStage &stage, EntityId eid, TimerComponent &timer)
{
  timer.time += stage.dt;
  if (timer.time >= timer.period)
  {
    timer.time = 0.f;
    g_mgr->sendEvent(eid, EventOnSpawn{});
  }
}
REG_SYS_2(spawner, "timer");

void render(
  const RenderStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const PositionComponent &pos)
{
  DrawCircleV(pos.p, 10, CLITERAL{ color.r, color.g, color.b, 255 });
}
REG_SYS_2(render, "color", "pos");

void spawn_handler(
  const EventOnSpawn &ev,
  EntityId eid,
  const VelocityComponent &vel,
  const PositionComponent &pos)
{
  JDocument doc;
  auto &a = doc.GetAllocator();

  const float sx = (float)GetRandomValue(-50, 50);
  const float sy = (float)GetRandomValue(-50, 50);

  JValue posValue(rapidjson::kObjectType);
  posValue.AddMember("x", pos.p.x, a);
  posValue.AddMember("y", pos.p.y, a);

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