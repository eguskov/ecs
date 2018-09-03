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
#define DEF_EMPTY_COMP(type, name) { bool set(const JValue&) { return true; }; } __attribute__((annotate("@component: " #name)));
#else
#define DEF_COMP(type, name) ;REG_COMP(type, name);
#define DEF_EMPTY_COMP(type, name) { bool set(const JValue&) { return true; }; };REG_COMP(type, name);
#endif

#define DEF_SET bool set(const JValue&) { return true; }

#define REG_COMP(type, n) \
  template <> struct Desc<type> { constexpr static size_t Size = sizeof(type); constexpr static char const* typeName = #type; constexpr static char const* name = #n; }; \

#define REG_COMP_ARR(type, n, sz) \
  template <> struct Desc<ArrayComp<type, sz>> { constexpr static size_t Size = sizeof(type) * sz; constexpr static char const* typeName = #type; constexpr static char const* name = __C4(n, [, sz, ]); }; \

#define REG_COMP_INIT(type, n) \
  static RegCompSpec<type> _##n(#n); \
  template <> int RegCompSpec<type>::ID = -1; \

#define REG_COMP_ARR_INIT(type, n, sz) \
  static RegCompSpec<ArrayComp<type, sz>> _array_##n(__C4(n, [, sz, ])); \
  template <> int RegCompSpec<ArrayComp<type, sz>>::ID = -1; \

#define REG_COMP_AND_INIT(type, n) \
  REG_COMP(type, n); \
  REG_COMP_INIT(type, n); \

template <typename T>
struct Desc;

struct RegComp
{
  char *name = nullptr;

  int id = -1;
  int size = 0;

  const RegComp *next = nullptr;

  RegComp(const char *_name, int _size);
  virtual ~RegComp();

  virtual bool init(uint8_t *mem, const JValue &value) const = 0;

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
struct CompSetter;

template <typename T>
struct CompSetter<T, true>
{
  static inline bool set(T *comp, const JValue &value)
  {
    return comp->set(value);
  }
};

template <typename T>
struct Setter;

template <typename T>
struct CompSetter<T, false>
{
  static inline bool set(T *comp, const JValue &value)
  {
    return Setter<T>::set(*comp, value);
  }
};

template <typename T, size_t Size>
struct ArrayComp
{
  using ArrayDesc = Desc<ArrayComp<T, Size>>;
  using ItemType = T;
  using ItemDesc = Desc<T>;

  eastl::array<ItemType, Size> items;

  ArrayComp()
  {
  }

  bool set(const JValue &value)
  {
    assert(value.IsArray());
    for (int i = 0; i < Size; ++i)
      if (!CompSetter<ItemType>::set(&items[i], value[i]))
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

template <typename T>
struct RegCompSpec : RegComp
{
  using CompType = T;
  using CompDesc = Desc<T>;

  static int ID;

  bool init(uint8_t *mem, const JValue &value) const override final
  {
    return CompSetter<CompType>::set((CompType*)mem, value["$value"]);
  }

  Storage* createStorage() const override final
  {
    return new StorageSpec<CompType>;
  }

  RegCompSpec(const char *name) : RegComp(name, CompDesc::Size)
  {
    ID = id;
  }
};

const RegComp *find_comp(const char *name);