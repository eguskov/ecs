#include "ecs/ecs.h"

#include "update.h"

#include <windows.h>

#include <random>
#include <ctime>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/position.component.h"
#include "components/velocity.component.h"

REG_EVENT_INIT(EventOnTest);
REG_EVENT_INIT(EventOnAnotherTest);

void update_position(const UpdateStage &stage,
  EntityId eid,
  const VelocityComponent &vel,
  PositionComponent &pos,
  const PositionComponent &pos1)
{
  std::srand(unsigned(std::time(0)));

  pos.x += ((float)std::rand() / RAND_MAX) * vel.x * stage.dt;
  pos.y += ((float)std::rand() / RAND_MAX) * vel.y * stage.dt;
}
REG_SYS_2(update_position, "vel", "pos", "pos1");

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
  PositionComponent &pos,
  const PositionComponent &pos1)
{
  std::cout << "test_handler(" << eid.handle << ")" << std::endl;
}
REG_SYS_2(test_handler, "vel", "pos", "pos1");

void test_another_handler(const EventOnAnotherTest &ev,
  EntityId eid,
  const VelocityComponent &vel,
  const PositionComponent &pos1)
{
  std::cout << "test_another_handler(" << eid.handle << ")" << std::endl;
}
REG_SYS_2(test_another_handler, "vel", "pos1");