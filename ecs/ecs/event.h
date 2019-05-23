#pragma once

#include "hash.h"

#ifdef __CODEGEN__
#define ECS_EVENT(type) struct ecs_event_##type { static constexpr char const *name = #type; };
#else
#define ECS_EVENT(type) template <> struct EventType<type> { constexpr static char const* eventName = #type; constexpr static uint32_t id = HASH(#type).hash; };
#endif

template <typename T>
struct EventType;
