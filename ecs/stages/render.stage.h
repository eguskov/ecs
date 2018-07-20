#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

struct RenderStage : Stage
{
};
REG_STAGE(RenderStage, render_stage);
