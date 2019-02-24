#include <ecs/ecs.h>

#include <stages/update.stage.h>
#include <stages/dispatchEvent.stage.h>

#include <script-ecs.h>

#include "script-update.h"

struct reload_script_handler
{
  ECS_RUN(const CmdReloadScript &ev, const EntityId &eid, script::ScriptComponent &script)
  {
    script.scriptECS.build(script.name.c_str(), script.path.c_str());
  }
};

struct build_script_handler
{
  ECS_RUN(const EventOnEntityCreate &ev, script::ScriptComponent &script)
  {
    script.scriptECS.build(script.name.c_str(), script.path.c_str());
  }
};

struct update_script_query
{
  ECS_RUN(const EventOnChangeDetected &ev, script::ScriptComponent &script)
  {
    script.scriptECS.invalidateQueries();
  }
};

struct update_script
{
  ECS_RUN(const UpdateStage &stage, script::ScriptComponent &script)
  {
    script.scriptECS.tick(stage);
  }
};

struct dispatch_event_script
{
  ECS_RUN(const DispatchEventStage &stage, script::ScriptComponent &script)
  {
    script.scriptECS.sendEventSync(stage.eid, stage.eventId, stage.ev);
  }
};

struct dispatch_broadcast_event_script
{
  ECS_RUN(const DispatchBroadcastEventStage &stage, script::ScriptComponent &script)
  {
    script.scriptECS.sendBroadcastEventSync(stage.eventId, stage.ev);
  }
};