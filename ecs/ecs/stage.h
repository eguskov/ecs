#pragma once

#define REG_STAGE(type) \
  template <> struct Desc<type> { constexpr static char const* typeName = #type; constexpr static char const* name = #type; }; \
  template <> \
  struct RegCompSpec<type> : RegComp \
  { \
    using CompType = type; \
    using CompDesc = Desc<type>; \
    static int ID; \
    bool init(uint8_t *, const JFrameValue &) const override final { return true; } \
    RegCompSpec() : RegComp(#type, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_STAGE_INIT(type) \
  static RegCompSpec<type> _reg_stage_##type; \
  int RegCompSpec<type>::ID = -1; \

#define REG_STAGE_AND_INIT(type) \
  REG_STAGE(type) \
  REG_STAGE_INIT(type) \

struct Stage
{
};