#pragma once

#include "stdafx.h"

#include <array>

#include "io/json.h"

#define __S(a) #a
#define __C2(a, b) __S(a) __S(b)
#define __C3(a, b, c) __C2(a, b) __S(c)
#define __C4(a, b, c, d) __C3(a, b, c) __S(d)

#define REG_COMP(type, n) \
  template <> struct Desc<type> { constexpr static size_t Size = sizeof(type); constexpr static char* typeName = #type; constexpr static char* name = #n; }; \

#define REG_COMP_ARR(type, n, sz) \
  template <> struct Desc<ArrayComp<type, sz>> { constexpr static size_t Size = sizeof(type) * sz; constexpr static char* typeName = #type; constexpr static char* name = __C4(n, [, sz, ]); }; \

#define REG_COMP_INIT(type, n) \
  static RegCompSpec<type> _##n(#n); \

#define REG_COMP_ARR_INIT(type, n, sz) \
  static RegCompSpec<ArrayComp<type, sz>> _array_##n(__C4(n, [, sz, ])); \

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
};

template <typename T, size_t Size>
struct ArrayComp
{
  using ArrayDesc = Desc<ArrayComp<T, Size>>;
	using ItemType = T;
  using ItemDesc = Desc<T>;

  std::array<ItemType, Size> items;

  ArrayComp()
  {
  }

  bool set(const JValue &value)
  {
    const JValue &arr = value["$array"];
    for (int i = 0; i < Size; ++i)
      items[i].set(arr[i]);
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

  bool init(uint8_t *mem, const JValue &value) const override final
  {
    CompType *comp = new (mem) CompType;
    return comp->set(value);
  }

  RegCompSpec(const char *name) : RegComp(name, CompDesc::Size)
  {
  }
};

const RegComp *find_comp(const char *name);