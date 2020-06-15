#pragma once

#include "stdafx.h"

#define __S(a) #a
#define __C2(a, b) __S(a) __S(b)
#define __C3(a, b, c) __C2(a, b) __S(c)
#define __C4(a, b, c, d) __C3(a, b, c) __S(d)

#ifdef __CODEGEN__

#define ECS_COMPONENT_TYPE(type) struct ecs_component_##type { static constexpr char const *name = #type; };
#define ECS_COMPONENT_TYPE_BIND(type) struct ecs_component_##type { static constexpr char const *name = #type; };
#define ECS_COMPONENT_TYPE_ALIAS(type, alias) struct ecs_component_##type { static constexpr char const *name = #alias; };

#else

#define ECS_COMPONENT_TYPE(T) \
  template <> struct ComponentType<T> \
  { \
    constexpr static uint32_t type = HASH(#T).hash; \
    constexpr static size_t size = sizeof(T); \
    constexpr static char const* typeName = #T; \
    constexpr static char const* name = #T; \
  }; \

#define ECS_COMPONENT_TYPE_BIND(T) \
  MAKE_TYPE_FACTORY(T, ::T); \
  template <> struct ComponentType<T> \
  { \
    constexpr static uint32_t type = HASH(#T).hash; \
    constexpr static size_t size = sizeof(T); \
    constexpr static char const* typeName = #T; \
    constexpr static char const* name = #T; \
  }; \

#define ECS_COMPONENT_TYPE_ALIAS(T, alias) \
  template <> struct ComponentType<T> \
  { \
    constexpr static uint32_t type = HASH(#T).hash; \
    constexpr static size_t size = sizeof(T); \
    constexpr static char const* typeName = #T; \
    constexpr static char const* name = #alias; \
  }; \

#endif

#define ECS_COMPONENT_TYPE_DETAILS(type) static ComponentDescriptionDetails<type> _##type(#type);
#define ECS_COMPONENT_TYPE_DETAILS_ALIAS(type, alias) static ComponentDescriptionDetails<type> _##alias(#alias);

#define ECS_DEFAULT_CTORS(T) \
  T() = default; \
  T(T &&assign) = default; \
  T(const T&) = default; \
  T& operator=(T &&assign) = default; \
  T& operator=(const T &assign) = default; \

struct ConstHashedString;

struct Storage;
template <typename T> struct StorageSpec;

template <typename T>
struct ComponentType;

// TODO: Add copy-flags: memcpy, move, etc.
struct ComponentDescription
{
  char *name = nullptr;
  uint32_t typeHash = 0;

  int id = -1;
  uint32_t size = 0;

  bool hasEqual = false;

  static const ComponentDescription *head;
  static int count;

  const ComponentDescription *next = nullptr;

  ComponentDescription(const char *_name, uint32_t _type_hash, uint32_t _size);
  virtual ~ComponentDescription();

  virtual void ctor(uint8_t *mem) const = 0;
  virtual void dtor(uint8_t *mem) const = 0;
  virtual bool equal(uint8_t *lhs, uint8_t *rhs) const = 0;
  virtual void copy(uint8_t *to, const uint8_t *from) const = 0;
  virtual void move(uint8_t *to, uint8_t *from) const = 0;
};

template<class T, class EqualTo>
struct HasOperatorEqualImpl
{
  template<class U, class V>
  static auto test(U*) -> decltype(eastl::declval<U>() == eastl::declval<V>());
  template<typename, typename>
  static auto test(...) -> eastl::false_type;

  using type = typename eastl::is_same<bool, decltype(test<T, EqualTo>(0))>::type;
};

template<class T, class EqualTo = T>
struct HasOperatorEqual : HasOperatorEqualImpl<T, EqualTo>::type {};

template <typename T, bool HasEqual>
struct ComponentComparator;

template <typename T>
struct ComponentComparator<T, true>
{
  static bool equal(const T &lhs, const T &rhs)
  {
    return lhs == rhs;
  }
};

template <typename T>
struct ComponentComparator<T, false>
{
  static bool equal(const T &lhs, const T &rhs)
  {
    return false;
  }
};

template <typename T>
struct ComponentDescriptionDetails final : ComponentDescription
{
  using CompType = T;
  using CompDesc = ComponentType<T>;

  void ctor(uint8_t *mem) const override final
  {
    new (mem) T();
  }

  void dtor(uint8_t *mem) const override final
  {
    ((T*)mem)->~T();
  }

  bool equal(uint8_t *lhs, uint8_t *rhs) const override final
  {
    return ComponentComparator<T, HasOperatorEqual<T>::value>::equal(*(T*)lhs, *(T*)rhs);
  }

  void copy(uint8_t *to, const uint8_t *from) const override final
  {
    *(T*)to = *(T*)from;
  }

  void move(uint8_t *to, uint8_t *from) const override final
  {
    *(T*)to = eastl::move(*(T*)from);
  }

  ComponentDescriptionDetails(const char *name) : ComponentDescription(name, CompDesc::type, CompDesc::size)
  {
    hasEqual = HasOperatorEqual<T>::value;
  }
};

const ComponentDescription *find_component(const char *name);
const ComponentDescription *find_component(const ConstHashedString &name);
const ComponentDescription *find_component(uint32_t type_hash);