#pragma once

#include "ecs/component.h"
#include "ecs/stage.h"
#include "ecs/entity.h"
#include "ecs/system.h"

struct DispatchEventStage
{
  EntityId eid;
  int eventId = -1;
  RawArg ev;

  DispatchEventStage() = default;
  DispatchEventStage(EntityId _eid, int _event_id, RawArg _ev) : eid(_eid), eventId(_event_id), ev(_ev) {}
};

ECS_STAGE(DispatchEventStage);

struct DispatchBroadcastEventStage
{
  int eventId = -1;
  RawArg ev;

  DispatchBroadcastEventStage() = default;
  DispatchBroadcastEventStage(int _event_id, RawArg _ev) : eventId(_event_id), ev(_ev) {}
};

ECS_STAGE(DispatchBroadcastEventStage);