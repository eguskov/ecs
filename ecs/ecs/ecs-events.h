#pragma once

#include "event.h"
#include "system.h"
#include "component.h"
#include "autoBind.h"

#include <daScript/daScript.h>

struct EventUpdate
{
  ECS_BIND_TYPE(ecs, isLocal=true);
  ECS_BIND_ALL_FIELDS;

  float dt;
  double total;
};

ECS_EVENT(EventUpdate);

struct EventRender
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventRender);

struct EventRenderDebug
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventRenderDebug);

struct EventRenderHUD
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventRenderHUD);

struct EventOnEntityCreate
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventOnEntityCreate);

struct EventOnEntityDelete
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventOnEntityDelete);

struct EventOnEntityReady
{
  ECS_BIND_TYPE(ecs, isLocal=true);
};

ECS_EVENT(EventOnEntityReady);
