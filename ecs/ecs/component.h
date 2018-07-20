#pragma once

#include "stdafx.h"

#include "io/json.h"

#define REG_COMP(type, n) \
  template <> struct Desc<type> { constexpr static char* typeName = #type; constexpr static char* name = #n; }; \

#define REG_COMP_INIT(type, n) \
  static RegCompSpec<type> _##n(#n); \

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

  RegCompSpec(const char *name) : RegComp(name, sizeof(CompType))
  {
  }
};

const RegComp *find_comp(const char *name);