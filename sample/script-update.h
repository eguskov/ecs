#pragma once

#include <ecs/event.h>
#include <ecs/component.h>

struct CmdReloadScript : Event
{
}
DEF_EVENT(CmdReloadScript);