#pragma once

#include "ecs/component.h"
#include "ecs/entity.h"
#include "ecs/hash.h"

template <>
struct Setter<bool>
{
  static inline bool set(bool &v, const JFrameValue &value)
  {
    ASSERT(value.IsBool());
    v = value.GetBool();
    return true;
  }
};

template <>
struct Setter<int>
{
  static inline bool set(int &v, const JFrameValue &value)
  {
    ASSERT(value.IsNumber());
    v = value.GetInt();
    return true;
  }
};

template <>
struct Setter<float>
{
  static inline bool set(float &v, const JFrameValue &value)
  {
    ASSERT(value.IsNumber());
    v = value.GetFloat();
    return true;
  }
};

template <>
struct Setter<glm::vec2>
{
  static inline bool set(glm::vec2 &vec, const JFrameValue &value)
  {
    ASSERT(value.IsArray() && value.Size() >= 2);
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
    ASSERT(value.IsArray() && value.Size() >= 3);
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
    ASSERT(value.IsArray() && value.Size() >= 4);
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
    ASSERT(value.IsInt());
    eid = value.GetInt();
    return true;
  }
};

template <>
struct Setter<eastl::string>
{
  static inline bool set(eastl::string &s, const JFrameValue &value)
  {
    ASSERT(value.IsString());
    s = value.GetString();
    return true;
  }
};

template <>
struct Setter<StringHash>
{
  static inline bool set(StringHash &s, const JFrameValue &value)
  {
    ASSERT(value.IsString());
    s.hash = hash::str(value.GetString());
    return true;
  }
};

struct Tag {};
ECS_COMPONENT_TYPE_ALIAS(Tag, tag);

ECS_COMPONENT_TYPE_ALIAS(EntityId, eid);
ECS_COMPONENT_TYPE(bool);
ECS_COMPONENT_TYPE(int);
ECS_COMPONENT_TYPE(float);
ECS_COMPONENT_TYPE_ALIAS(glm::vec2, vec2);
ECS_COMPONENT_TYPE_ALIAS(glm::vec3, vec3);
ECS_COMPONENT_TYPE_ALIAS(glm::vec4, vec4);
ECS_COMPONENT_TYPE_ALIAS(eastl::string, string);
ECS_COMPONENT_TYPE_ALIAS(StringHash, hash_string);

// TODO: Remove?
// REG_COMP_ARR(glm::vec2, vec2, 2);