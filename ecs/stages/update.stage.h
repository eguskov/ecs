#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

struct UpdateStage : Stage
{
  float dt = 0.f;
  float total = 0.f;
  UpdateStage() = default;
  UpdateStage(float _dt, float _total) : dt(_dt), total(_total) {}
};
REG_STAGE(UpdateStage);
