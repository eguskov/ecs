#include "ecs/ecs.h"

#include "components/position.component.h"
#include "components/velocity.component.h"

void update_position(const UpdateStage &stage, EntityId eid, const VelocityComponent &velocity, PositionComponent &p, const PositionComponent &p1)
{
  std::cout << "update_position(" << eid.handle << ")" << std::endl;
  p.x += velocity.x * stage.dt;
  p.y += velocity.y * stage.dt;
}
REG_SYS_2(update_position, "v", "p", "p1");

void position_printer(const UpdateStage &stage, EntityId eid, const PositionComponent &p, const PositionComponent &p1)
{
  std::cout << "position_printer(" << eid.handle << ")" << std::endl;
  std::cout << "(" << p.x << ", " << p.y << ")" << std::endl;
}
REG_SYS_2(position_printer, "p", "p1");