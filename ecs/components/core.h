#pragma once

#include "ecs/component.h"
#include "ecs/entity.h"

template <> struct Desc<EntityId> { constexpr static char* typeName = "EntityId"; constexpr static char* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  static int ID;

  bool init(uint8_t *, const JValue &) const override final { return true; }

  RegCompSpec() : RegComp("eid", sizeof(CompType))
  {
    ID = id;
  }
};

template <>
struct Setter<int>
{
  static inline bool set(int &v, const JValue &value)
  {
    assert(value.IsInt());
    v = value.GetInt();
    return true;
  }
};

template <>
struct Setter<float>
{
  static inline bool set(float &v, const JValue &value)
  {
    assert(value.IsFloat());
    v = value.GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec2>
{
  static inline bool set(glm::vec2 &vec, const JValue &value)
  {
    assert(value.HasMember("x"));
    assert(value.HasMember("y"));
    vec.x = value["x"].GetFloat();
    vec.y = value["y"].GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec3>
{
  static inline bool set(glm::vec3 &vec, const JValue &value)
  {
    assert(value.HasMember("x"));
    assert(value.HasMember("y"));
    assert(value.HasMember("z"));
    vec.x = value["x"].GetFloat();
    vec.y = value["y"].GetFloat();
    vec.z = value["z"].GetFloat();
    return true;
  }
};

REG_COMP(int, int);
REG_COMP(float, float);
REG_COMP(glm::vec2, vec2);
REG_COMP(glm::vec3, vec3);
REG_COMP_ARR(glm::vec2, vec2, 2);