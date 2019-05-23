#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"

struct RenderStage
{
};

ECS_STAGE(RenderStage);

struct RenderDebugStage
{
};

ECS_STAGE(RenderDebugStage);

struct RenderHUDStage
{
};

ECS_STAGE(RenderHUDStage);