#include "ecs/ecs.h"

#include "update.h"

#include <windows.h>

#include <random>
#include <ctime>
#include <future>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/position.component.h"
#include "components/velocity.component.h"
#include "components/timer.component.h"
#include "components/color.component.h"

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

struct ScreenSizeComponent
{
  int width = 0;
  int height = 0;

  ScreenSizeComponent()
  {
    
  }

  bool set(const JValue &value)
  {
    return true;
  }
};
REG_COMP_AND_INIT(ScreenSizeComponent, screenSize);

struct ContextComponent
{
  HWND console = NULL;
  HDC dc = NULL;

  ContextComponent()
  {
    console = ::GetConsoleWindow();
    dc = ::GetDC(console);
  }

  ~ContextComponent()
  {
    ::ReleaseDC(console, dc);
  }

  bool set(const JValue &value)
  {
    return true;
  }
};
REG_COMP_AND_INIT(ContextComponent, context);

void screen_size_create_handler(const EventOnEntityCreate &ev,
  EntityId eid,
  const ContextComponent &ctx,
  ScreenSizeComponent &sz)
{
  g_mgr->waitFor(eid, std::async(std::launch::async, []()
  {
    ::Sleep(10 * 1000);
    return true;
  }));
}
REG_SYS_2(screen_size_create_handler, "context", "screenSize");

void screen_size_ready_handler(const EventOnEntityReady &ev,
  EntityId eid,
  const ContextComponent &ctx,
  ScreenSizeComponent &sz)
{
  HWND console = ::GetConsoleWindow();
  HDC dc = ::GetDC(console);

  BITMAP structBitmapHeader;
  memset(&structBitmapHeader, 0, sizeof(BITMAP));

  HGDIOBJ hBitmap = GetCurrentObject(dc, OBJ_BITMAP);
  GetObject(hBitmap, sizeof(BITMAP), &structBitmapHeader);

  sz.width = structBitmapHeader.bmWidth;
  sz.height = structBitmapHeader.bmHeight;

  ::ReleaseDC(console, dc);
}
REG_SYS_2(screen_size_ready_handler, "context", "screenSize");

void clear_screen(const RenderStage &stage,
  const ContextComponent &ctx,
  const ScreenSizeComponent &sz)
{
  return;
  COLORREF COLOR = RGB(0, 0, 0);
  for (int x = 0; x < sz.width; ++x)
    for (int y = 0; y < sz.height; ++y)
      ::SetPixel(ctx.dc, x, y, COLOR);
}
REG_SYS_1(clear_screen, "context", "screenSize");

void render(const RenderStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const PositionComponent &pos)
{
  std::vector<EntityId> eids;
  g_mgr->query(eids, { {"context", "context" } });

  const auto &ctx = g_mgr->getComponent<ContextComponent>(eids[0], "context");

  COLORREF COLOR = RGB(color.r, color.g, color.b);
  for (int x = 0; x < 10; ++x)
    for (int y = 0; y < 10; ++y)
      ::SetPixel(ctx.dc, (int)pos.x + x, (int)pos.y + y, COLOR);
}
REG_SYS_2(render, "color", "pos");

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

  JValue colorValue(rapidjson::kObjectType);
  colorValue.AddMember("r", 255, a);
  colorValue.AddMember("g", 0, a);
  colorValue.AddMember("b", 0, a);

  JValue value(rapidjson::kObjectType);
  value.AddMember("pos", posValue, a);
  value.AddMember("color", colorValue, a);
  
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