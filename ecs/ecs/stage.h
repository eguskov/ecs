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
    RegCompSpec() : RegComp(#n, reg_comp_count, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_STAGE_INIT(type, n) \
  static RegCompSpec<type> _##n; \
  int RegCompSpec<type>::ID = -1; \

struct Stage
{
};