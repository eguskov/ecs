#pragma once

#include "hash.h"

#define ECS_STAGE(type) template <> struct StageType<type> { constexpr static char const* eventName = #type; constexpr static uint32_t id = HASH(#type).hash; };

template <typename T>
struct StageType;