#include <ecs/ecs.h>

#include "update.ecs.h"

#include <stages/update.stage.h>

struct update_position
{
  ECS_RUN(const UpdateStage &stage, const glm::vec3 &vel, glm::vec3 &pos)
  {
    pos += vel * stage.dt;
  }
};
