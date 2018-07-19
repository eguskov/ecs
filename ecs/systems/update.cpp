#include "ecs/ecs.h"

#include "components/position.component.h"
#include "components/velocity.component.h"

void update_position(const UpdateStage &stage, EntityId eid, const VelocityComponent &velocity, PositionComponent &position)
{
  std::cout << "update_position(" << eid.handle << ")" << std::endl;
  position.x += velocity.x * stage.dt;
  position.y += velocity.y * stage.dt;
}
REG_SYS(update_position);

void position_printer(const UpdateStage &stage, EntityId eid, const PositionComponent &position)
{
  std::cout << "position_printer(" << eid.handle << ")" << std::endl;
  std::cout << "(" << position.x << ", " << position.y << ")" << std::endl;
}
REG_SYS(position_printer);