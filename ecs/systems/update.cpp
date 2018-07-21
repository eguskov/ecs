#include "ecs/ecs.h"

#include "update.h"

#include <windows.h>

#include <random>
#include <ctime>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/position.component.h"
#include "components/velocity.component.h"
#include "components/timer.component.h"

REG_EVENT_INIT(EventOnTest);
REG_EVENT_INIT(EventOnAnotherTest);

void update_position(const UpdateStage &stage,
  const VelocityComponent &vel,
  PositionComponent &pos)
{
  pos.x += 2.f * vel.x * stage.dt;
  pos.y += 2.f * vel.y * stage.dt;
}
REG_SYS_1(update_position, "vel", "pos");

void update_vels(const UpdateStage &stage,
  const ArrayComp<VelocityComponent, 2> &vels)
{
  vels[0].x;
}
REG_SYS_1(update_vels, "vels");

void update_velocity(const UpdateStage &stage,
  EntityId eid,
  VelocityComponent &vel,
  TimerComponent &timer)
{
  timer.time += stage.dt;
  if (timer.time >= timer.period)
  {
    timer.time = 0.f;
    vel.y *= 0.2f + ((float)std::rand() / RAND_MAX);

    // It works!
    g_mgr->sendEvent(eid, EventOnTest{0.f, 0.f});
  }
}
REG_SYS_2(update_velocity, "vel", "timer");

void render(const RenderStage &stage,
  EntityId eid,
  const PositionComponent &pos)
{
  HWND console = ::GetConsoleWindow();
  HDC dc = ::GetDC(console);

  COLORREF COLOR = RGB(255, 255, 255);

  ::SetPixel(dc, (int)pos.x, (int)pos.y, COLOR);

  ::ReleaseDC(console, dc);
}
REG_SYS_2(render, "pos");

//void position_printer(const UpdateStage &stage,
//  EntityId eid,
//  const PositionComponent &pos,
//  const PositionComponent &pos1)
//{
//  std::cout << "position_printer(" << eid.handle << ")" << std::endl;
//  std::cout << "(" << pos.x << ", " << pos.y << ")" << std::endl;
//}
//REG_SYS_2(position_printer, "pos", "pos1");

void test_handler(const EventOnTest &ev,
  EntityId eid,
  const VelocityComponent &vel,
  const PositionComponent &pos)
{
  JDocument doc;
  auto &a = doc.GetAllocator();

  JValue posValue(rapidjson::kObjectType);
  posValue.AddMember("x", pos.x, a);
  posValue.AddMember("y", pos.y, a);

  JValue value(rapidjson::kObjectType);
  value.AddMember("pos", posValue, a);
  
  g_mgr->createEntity("test", value);
}
REG_SYS_2(test_handler, "vel", "pos");

void test_another_handler(const EventOnAnotherTest &ev,
  EntityId eid,
  const VelocityComponent &vel,
  const PositionComponent &pos1)
{
  std::cout << "test_another_handler(" << eid.handle << ")" << std::endl;
}
REG_SYS_2(test_another_handler, "vel", "pos1");