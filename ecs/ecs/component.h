#pragma once

#include "stdafx.h"

#include "ecs/storage.h"

#include "io/json.h"

#define __S(a) #a
#define __C2(a, b) __S(a) __S(b)
#define __C3(a, b, c) __C2(a, b) __S(c)
#define __C4(a, b, c, d) __C3(a, b, c) __S(d)

#ifdef __CODEGEN__
#define DEF_COMP(type, name) __attribute__((annotate("@component: " #name)))
#define DEF_EMPTY_COMP(type, name) { bool set(const JFrameValue&) { return true; }; } __attribute__((annotate("@component: " #name)));
#else
#define DEF_COMP(type, name) ;REG_COMP(type, name);
#define DEF_EMPTY_COMP(type, name) { bool set(const JFrameValue&) { return true; }; };REG_COMP(type, name);
#endif

#define DEF_SET bool set(const JFrameValue&) { return true; }

#define REG_COMP(type, n) \
  template <> struct Desc<type> { constexpr static size_t Size = sizeof(type); constexpr static char const* typeName = #type; constexpr static char const* name = #n; }; \

#define REG_COMP_ARR(type, n, sz) \
  template <> struct Desc<ArrayComponent<type, sz>> { constexpr static size_t Size = sizeof(type) * sz; constexpr static char const* typeName = #type; constexpr static char const* name = __C4(n, [, sz, ]); }; \

#define REG_COMP_INIT(type, n) \
  static ComponentDescriptionDetails<type> _##n(#n); \
  template <> int ComponentDescriptionDetails<type>::ID = -1; \

#define REG_COMP_ARR_INIT(type, n, sz) \
  static ComponentDescriptionDetails<ArrayComponent<type, sz>> _array_##n(__C4(n, [, sz, ])); \
  template <> int ComponentDescriptionDetails<ArrayComponent<type, sz>>::ID = -1; \

#define REG_COMP_AND_INIT(type, n) \
  REG_COMP(type, n); \
  REG_COMP_INIT(type, n); \

template <typename T>
struct Desc;

struct ComponentDescription
{
  char *name = nullptr;

  int id = -1;
  int size = 0;

  bool hasEqual = false;

  const ComponentDescription *next = nullptr;

  ComponentDescription(const char *_name, int _size);
  virtual ~ComponentDescription();

  virtual bool init(uint8_t *mem, const JFrameValue &value) const = 0;
  virtual bool equal(uint8_t *lhs, uint8_t *rhs) const = 0;

  virtual Storage* createStorage() const { return nullptr; }
};

template <typename T>
struct HasSetMethod
{
  struct has { char d[1]; };
  struct notHas { char d[2]; };
  template <typename C> static has test(decltype(&C::set));
  template <typename C> static notHas test(...);
  static constexpr bool value = sizeof(test<T>(0)) == sizeof(has);
};

template <typename T, bool HasSet = HasSetMethod<T>::value>
struct ComponentSetter;

template <typename T>
struct ComponentSetter<T, true>
{
  static inline bool set(T *comp, const JFrameValue &value)
  {
    return comp->set(value);
  }
};

template <typename T>
struct Setter;

template <typename T>
struct ComponentSetter<T, false>
{
  static inline bool set(T *comp, const JFrameValue &value)
  {
    return Setter<T>::set(*comp, value);
  }
};

template <typename T, size_t Size>
struct ArrayComponent
{
  using ArrayDesc = Desc<ArrayComponent<T, Size>>;
  using ItemType = T;
  using ItemDesc = Desc<T>;

  eastl::array<ItemType, Size> items;

  ArrayComponent()
  {
  }

  bool set(const JFrameValue &value)
  {
    ASSERT(value.IsArray());
    for (int i = 0; i < Size; ++i)
      if (!ComponentSetter<ItemType>::set(&items[i], value[i]))
        return false;
    return true;
  }

  ItemType& operator[](int i)
  {
    return items[i];
  }

  const ItemType& operator[](int i) const
  {
    return items[i];
  }
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
struct ComponentDescriptionDetails : ComponentDescription
{
  using CompType = T;
  using CompDesc = Desc<T>;

  static int ID;

  bool init(uint8_t *mem, const JFrameValue &value) const override final
  {
    return ComponentSetter<CompType>::set((CompType*)mem, value["$value"]);
  }

  bool equal(uint8_t *lhs, uint8_t *rhs) const override final
  {
    return ComponentComparator<T, HasOperatorEqual<T>::value>::equal(*(T*)lhs, *(T*)rhs);
  }

  Storage* createStorage() const override final
  {
    return new StorageSpec<CompType>;
  }

  ComponentDescriptionDetails(const char *name) : ComponentDescription(name, CompDesc::Size)
  {
    ID = id;
    hasEqual = HasOperatorEqual<T>::value;
  }
};

const ComponentDescription *find_component(const char *name);