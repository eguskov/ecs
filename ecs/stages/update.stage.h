#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

extern int reg_comp_count;

struct UpdateStage : Stage
{
  float dt;
  UpdateStage(float _dt) : dt(_dt) {}
};
REG_STAGE(UpdateStage, update_stage);
