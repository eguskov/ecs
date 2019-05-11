#include "grid.h"

#include <ecs/ecs.h>
#include <ecs/jobmanager.h>

#include <stages/update.stage.h>

struct update_grid_cell
{
  ECS_RUN(const UpdateStage &stage, const glm::vec2 &cur_pos, int &grid_cell)
  {
    grid_cell = MAKE_GRID_CELL_FROM_POS(cur_pos, GRID_CELL_SIZE);
  }
};

struct update_small_grid_cell
{
  ECS_RUN(const UpdateStage &stage, const glm::vec2 &cur_pos, int &small_grid_cell)
  {
    small_grid_cell = MAKE_GRID_CELL_FROM_POS(cur_pos, SMALL_GRID_CELL_SIZE);
  }
};