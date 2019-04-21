#include "grid.h"

#include <ecs/ecs.h>
#include <ecs/jobmanager.h>

#include <stages/update.stage.h>

struct update_grid_cell
{
  ECS_RUN_IN_JOBS(const UpdateStage &stage, const glm::vec2 &pos, int &grid_cell)
  {
    grid_cell = MAKE_GRID_CELL_FROM_POS(pos, GRID_CELL_SIZE);
  }
};

struct update_small_grid_cell
{
  ECS_RUN_IN_JOBS(const UpdateStage &stage, const glm::vec2 &pos, int &small_grid_cell)
  {
    small_grid_cell = MAKE_GRID_CELL_FROM_POS(pos, SMALL_GRID_CELL_SIZE);
  }
};