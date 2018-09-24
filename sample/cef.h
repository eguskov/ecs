#pragma once

#include <ecs/entity.h>
#include <ecs/event.h>

namespace cef
{
  struct CmdToggleDevTools : Event
  {
    CmdToggleDevTools() = default;
  }
  DEF_EVENT(CmdToggleDevTools);

  struct CmdToggleWebUI : Event
  {
    CmdToggleWebUI() = default;
  }
  DEF_EVENT(CmdToggleWebUI);

  struct EventOnClickOutside : Event
  {
    EventOnClickOutside() = default;
  }
  DEF_EVENT(EventOnClickOutside);

  EntityId get_eid();

  void init();
  void release();
  void update();
}