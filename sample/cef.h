#pragma once

#include <ecs/entity.h>
#include <ecs/event.h>

namespace cef
{
  struct CmdToggleDevTools
  {
    CmdToggleDevTools() = default;
  };

  ECS_EVENT(cef::CmdToggleDevTools);

  struct CmdToggleWebUI
  {
    CmdToggleWebUI() = default;
  };

  ECS_EVENT(cef::CmdToggleWebUI);

  struct EventOnClickOutside
  {
    EventOnClickOutside() = default;
  };

  ECS_EVENT(cef::EventOnClickOutside);

  EntityId get_eid();

  void init();
  void release();
  void update();
}