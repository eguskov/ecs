#pragma once

#include "component.h"

#ifdef __CODEGEN__
#define DEF_EVENT(...) __attribute__((annotate("@event")))
#else
#define DEF_EVENT(type) ;REG_EVENT(type)
#endif

#define REG_EVENT(type) \
  template <> struct Desc<type> { constexpr static char const* typeName = #type; constexpr static char const* name = #type; }; \
  template <> \
  struct ComponentDescriptionDetails<type> : ComponentDescription \
  { \
    using CompType = type; \
    using CompDesc = Desc<type>; \
    static int ID; \
    bool init(uint8_t *, const JFrameValue &) const override final { return true; } \
    bool equal(uint8_t *, uint8_t *) const override final { return false; } \
    ComponentDescriptionDetails() : ComponentDescription(#type, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_EVENT_INIT(type) \
  static ComponentDescriptionDetails<type> _##type; \
  int ComponentDescriptionDetails<type>::ID = -1; \

#define REG_EVENT_AND_INIT(type) \
  REG_EVENT(type); \
  REG_EVENT_INIT(type); \

struct Event
{
};