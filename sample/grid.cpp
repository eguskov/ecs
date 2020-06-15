#include "grid.h"

#include <ecs/ecs.h>
#include <ecs/jobmanager.h>

#include <stages/update.stage.h>

struct update_grid_cell
{
  ECS_AFTER(update_position);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &pos, int &grid_cell)
  {
    grid_cell = MAKE_GRID_CELL_FROM_POS(pos, GRID_CELL_SIZE);
  }
};