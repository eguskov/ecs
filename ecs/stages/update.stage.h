#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

struct UpdateStage
{
  float dt = 0.f;
  double total = 0.0;
  UpdateStage() = default;
  UpdateStage(float _dt, float _total) : dt(_dt), total(_total) {}
};

ECS_STAGE(UpdateStage);
