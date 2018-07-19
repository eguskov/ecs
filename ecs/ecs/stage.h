#pragma once

#define REG_STAGE(type, n) \
  template <> struct Desc<type> { constexpr static char* typeName = #type; constexpr static char* name = #n; }; \
  template <> \
  struct RegCompSpec<type> : RegComp \
  { \
    using CompType = type; \
    using CompDesc = Desc<type>; \
    static int ID; \
    void init(uint8_t *) const override final {} \
    RegCompSpec() : RegComp(#n, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_STAGE_INIT(type) \
  static RegCompSpec<type> _##n; \
  int RegCompSpec<type>::ID = -1; \

struct Stage
{
};