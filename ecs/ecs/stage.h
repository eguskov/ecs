#pragma once

#define REG_STAGE(type) \
  template <> struct ComponentType<type> { constexpr static char const* typeName = #type; constexpr static char const* name = #type; }; \
  template <> \
  struct ComponentDescriptionDetails<type> : ComponentDescription \
  { \
    using CompType = type; \
    using CompDesc = ComponentType<type>; \
    static int ID; \
    bool init(uint8_t *, const JFrameValue &) const override final { return true; } \
    bool equal(uint8_t *, uint8_t *) const override final { return false; } \
    ComponentDescriptionDetails() : ComponentDescription(#type, sizeof(CompType)) { ID = id; } \
  }; \

#define REG_STAGE_INIT(type) \
  static ComponentDescriptionDetails<type> _reg_stage_##type; \
  int ComponentDescriptionDetails<type>::ID = -1; \

#define REG_STAGE_AND_INIT(type) \
  REG_STAGE(type) \
  REG_STAGE_INIT(type) \

struct Stage
{
};