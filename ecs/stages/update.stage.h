#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

struct UpdateStage : Stage
{
  float dt;
  float total;
  UpdateStage(float _dt, float _total) : dt(_dt), total(_total) {}
};
REG_STAGE(UpdateStage, update_stage);
