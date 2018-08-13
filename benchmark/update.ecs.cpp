#include <ecs/ecs.h>

#include "update.ecs.h"

#include <stages/update.stage.h>

DEF_SYS()
static __forceinline void update_position(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
{
  pos += vel * stage.dt;
}
