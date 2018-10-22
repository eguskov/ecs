#include <ecs/ecs.h>

#include <stages/update.stage.h>

#include <scriptECS.h>

#include "script.ecs.h"

DEF_SYS()
static __forceinline void reload_script_handler(const CmdReloadScript &ev, script::ScriptComponent &script)
{
  script.scriptECS.build(script.name.c_str(), script.path.c_str());
}

DEF_SYS()
static __forceinline void build_script_handler(const EventOnEntityCreate &ev, script::ScriptComponent &script)
{
  script.scriptECS.build(script.name.c_str(), script.path.c_str());
}

DEF_SYS()
static __forceinline void update_script_query(const UpdateStage &stage, script::ScriptComponent &script)
{
  if (g_mgr->status != kStatusNone)
    script.scriptECS.invalidateQueries();
}

DEF_SYS()
static __forceinline void update_script(const UpdateStage &stage, script::ScriptComponent &script)
{
  script.scriptECS.tick(stage);
}