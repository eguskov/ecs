#pragma once

#include "ecs/component.h"
#include "ecs/entity.h"
#include "ecs/hash.h"

struct Tag {};
ECS_COMPONENT_TYPE(Tag);

ECS_COMPONENT_TYPE(EntityId);
ECS_COMPONENT_TYPE(bool);
ECS_COMPONENT_TYPE(int);
ECS_COMPONENT_TYPE(float);
ECS_COMPONENT_TYPE_ALIAS(glm::vec2, vec2);
ECS_COMPONENT_TYPE_ALIAS(glm::vec3, vec3);
ECS_COMPONENT_TYPE_ALIAS(glm::vec4, vec4);
ECS_COMPONENT_TYPE_ALIAS(eastl::string, string);