#pragma once

#include "ecs/component.h"
#include "ecs/entity.h"
#include "ecs/hash.h"

template <> struct Desc<EntityId> { constexpr static size_t Size = sizeof(EntityId); constexpr static char const* typeName = "EntityId"; constexpr static char const* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  static int ID;

  bool init(uint8_t *mem, const JFrameValue &value) const override final
  {
    return CompSetter<CompType>::set((CompType*)mem, value["$value"]);
  }

  Storage* createStorage() const override final
  {
    return new StorageSpec<CompType>;
  }

  RegCompSpec() : RegComp("eid", sizeof(CompType))
  {
    ID = id;
  }
};

template <>
struct Setter<bool>
{
  static inline bool set(bool &v, const JFrameValue &value)
  {
    assert(value.IsBool());
    v = value.GetBool();
    return true;
  }
};

template <>
struct Setter<int>
{
  static inline bool set(int &v, const JFrameValue &value)
  {
    assert(value.IsInt());
    v = value.GetInt();
    return true;
  }
};

template <>
struct Setter<float>
{
  static inline bool set(float &v, const JFrameValue &value)
  {
    assert(value.IsFloat());
    v = value.GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec2>
{
  static inline bool set(glm::vec2 &vec, const JFrameValue &value)
  {
    assert(value.IsArray() && value.Size() >= 2);
    vec.x = value[0].GetFloat();
    vec.y = value[1].GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec3>
{
  static inline bool set(glm::vec3 &vec, const JFrameValue &value)
  {
    assert(value.IsArray() && value.Size() >= 3);
    vec.x = value[0].GetFloat();
    vec.y = value[1].GetFloat();
    vec.z = value[2].GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec4>
{
  static inline bool set(glm::vec4 &vec, const JFrameValue &value)
  {
    assert(value.IsArray() && value.Size() >= 4);
    vec.x = value[0].GetFloat();
    vec.y = value[1].GetFloat();
    vec.z = value[2].GetFloat();
    vec.w = value[3].GetFloat();
    return true;
  }
};

template <>
struct Setter<EntityId>
{
  static inline bool set(EntityId &eid, const JFrameValue &value)
  {
    assert(value.IsInt());
    eid = value.GetInt();
    return true;
  }
};

template <>
struct Setter<eastl::string>
{
  static inline bool set(eastl::string &s, const JFrameValue &value)
  {
    assert(value.IsString());
    s = value.GetString();
    return true;
  }
};

template <>
struct Setter<HashedString>
{
  static inline bool set(HashedString &s, const JFrameValue &value)
  {
    assert(value.IsString());
    s.hash = hash::fnv1<uint32_t>::hash(value.GetString());
    s.str = value.GetString();
    return true;
  }
};

REG_COMP(bool, bool);
REG_COMP(int, int);
REG_COMP(float, float);
REG_COMP(glm::vec2, vec2);
REG_COMP(glm::vec3, vec3);
REG_COMP(glm::vec4, vec4);
REG_COMP(eastl::string, string);
REG_COMP(HashedString, hash_string);

// TODO: Remove?
REG_COMP_ARR(glm::vec2, vec2, 2);