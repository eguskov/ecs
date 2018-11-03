#include <ecs/ecs.h>

#include <stages/update.stage.h>
#include <stages/dispatchEvent.stage.h>

#include <scriptECS.h>

#include "script.ecs.h"

DEF_SYS()
static __forceinline void reload_script_handler(const CmdReloadScript &ev, const EntityId &eid, script::ScriptComponent &script)
{
  script.scriptECS.build(script.name.c_str(), script.path.c_str());
}

DEF_SYS()
static __forceinline void build_script_handler(const EventOnEntityCreate &ev, script::ScriptComponent &script)
{
  script.scriptECS.build(script.name.c_str(), script.path.c_str());
}

DEF_SYS()
static __forceinline void update_script_query(const EventOnChangeDetected &ev, script::ScriptComponent &script)
{
  script.scriptECS.invalidateQueries();
}

DEF_SYS()
static __forceinline void update_script(const UpdateStage &stage, script::ScriptComponent &script)
{
  script.scriptECS.tick(stage);
}

DEF_SYS()
static __forceinline void dispatch_event_script(const DispatchEventStage &stage, script::ScriptComponent &script)
{
  script.scriptECS.sendEventSync(stage.eid, stage.eventId, stage.ev);
}