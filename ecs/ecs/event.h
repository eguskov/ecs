#pragma once

#define REG_EVENT(type) \
  template <> struct Desc<type> { constexpr static char* typeName = #type; constexpr static char* name = #type; }; \
  template <> \
  struct RegCompSpec<type> : RegComp \
  { \
    using CompType = type; \
    using CompDesc = Desc<type>; \
    static int ID; \
    bool init(uint8_t *, const JValue &) const override final { return true; } \
    RegCompSpec() : RegComp(#type, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_EVENT_INIT(type) \
  static RegCompSpec<type> _##type; \
  int RegCompSpec<type>::ID = -1; \

struct Event
{
};