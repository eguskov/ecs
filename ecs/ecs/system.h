#pragma once

#include "stdafx.h"

#include "ecs/stage.h"
#include "ecs/event.h"
#include "ecs/storage.h"
#include "ecs/query.h"

struct EntityManager;

#define QL_EVAL0(...) __VA_ARGS__
#define QL_EVAL1(...) QL_EVAL0(QL_EVAL0(QL_EVAL0(__VA_ARGS__)))
#define QL_EVAL2(...) QL_EVAL1(QL_EVAL1(QL_EVAL1(__VA_ARGS__)))
#define QL_EVAL3(...) QL_EVAL2(QL_EVAL2(QL_EVAL2(__VA_ARGS__)))
#define QL_EVAL4(...) QL_EVAL3(QL_EVAL3(QL_EVAL3(__VA_ARGS__)))
#define QL_EVAL(...)  QL_EVAL4(QL_EVAL4(QL_EVAL4(__VA_ARGS__)))

#define QL_END(...)
#define QL_OUT
#define QL_COMMA ,

#define QL_FOREACH_GET_END2() 0, QL_END
#define QL_FOREACH_GET_END1(...) QL_FOREACH_GET_END2
#define QL_FOREACH_GET_END(...) QL_FOREACH_GET_END1

#define QL_FOREACH_NEXT0(test, next, ...) next QL_OUT
#define QL_FOREACH_NEXT1(test, next) QL_FOREACH_NEXT0(test, next, 0)
#define QL_FOREACH_NEXT(test, next)  QL_FOREACH_NEXT1(QL_FOREACH_GET_END test, next)

#define QL_FOREACH0(f, x, peek, ...) f(x) QL_FOREACH_NEXT(peek, QL_FOREACH1)(f, peek, __VA_ARGS__)
#define QL_FOREACH1(f, x, peek, ...) f(x) QL_FOREACH_NEXT(peek, QL_FOREACH0)(f, peek, __VA_ARGS__)

#define QL_FOREACH_COMMA_NEXT1(test, next) QL_FOREACH_NEXT0(test, QL_COMMA next, 0)
#define QL_FOREACH_COMMA_NEXT(test, next)  QL_FOREACH_COMMA_NEXT1(QL_FOREACH_GET_END test, next)

#define QL_FOREACH_COMMA0(f, x, peek, ...) f(x) QL_FOREACH_COMMA_NEXT(peek, QL_FOREACH_COMMA1)(f, peek, __VA_ARGS__)
#define QL_FOREACH_COMMA1(f, x, peek, ...) f(x) QL_FOREACH_COMMA_NEXT(peek, QL_FOREACH_COMMA0)(f, peek, __VA_ARGS__)

#define QL_FOREACH(f, ...) QL_EVAL(QL_FOREACH1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
#define QL_FOREACH_COMMA(f, ...) QL_EVAL(QL_FOREACH_COMMA1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#ifdef __CODEGEN__
  struct ql_component {};
  #define QL_COMPONENT(x) ql_component x;

  #define QL_HAVE(...) struct ql_have { QL_FOREACH(QL_COMPONENT, __VA_ARGS__) };
  #define QL_NOT_HAVE(...) struct ql_not_have { QL_FOREACH(QL_COMPONENT, __VA_ARGS__) };
  #define QL_WHERE(expr) struct ql_where { static constexpr char const *ql_expr = #expr; };
  #define QL_JOIN(expr) struct ql_join { static constexpr char const *ql_expr = #expr; };

  #define ECS_QUERY struct ecs_query {};
  #define ECS_SYSTEM struct ecs_system {};
#else
  #define QL_HAVE(...)
  #define QL_NOT_HAVE(...)
  #define QL_WHERE(...)
  #define QL_JOIN(...)

  #define ECS_QUERY template <typename Callable> static __forceinline void foreach(Callable);
  #define ECS_SYSTEM
#endif

#define ECS_RUN ECS_SYSTEM; static __forceinline void run

#define GET_COMPONENT_VALUE(c, t) t c = type.storages[type.getComponentIndex(HASH(#c))].storage->getByIndex<t>(entity_idx)
#define GET_COMPONENT_VALUE_ITER(c, t) auto it_##c = query.chunks[compIdx_##c + chunkIdx * query.componentsCount].begin<t>()
#define GET_COMPONENT_ITER(q, c, t) auto c = query.iter<t>(index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components))
#define GET_COMPONENT_INDEX(q, c) static constexpr int compIdx_##c = index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components)
#define GET_COMPONENT(q, i, t, c) i.get<t>(index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components))

struct RegSys;
extern RegSys *reg_sys_head;
extern int reg_sys_count;

struct RawArg
{
  int size = 0;
  uint8_t *mem = nullptr;
  RawArg(int _size = 0, uint8_t *_mem = nullptr) : size(_size), mem(_mem) {}
};

template <int Size>
struct RawArgSpec : RawArg
{
  eastl::array<uint8_t, Size> buffer;
  RawArgSpec() : RawArg(Size, &buffer[0]) {}
};

struct RegSys
{
  enum class Mode { FROM_INTERNAL_QUERY, FROM_EXTERNAL_QUERY };
  using SystemCallback = void (*)(const RawArg &stage_or_event, Query &query);

  char *name = nullptr;
  char *stageName = nullptr;

  int id = -1;
  int stageId = -1;

  Mode mode = Mode::FROM_INTERNAL_QUERY;

  const RegSys *next = nullptr;

  // TODO: Remove compMask
  eastl::bitvector<> compMask;
  ConstQueryDesc queryDesc;
  SystemCallback sys = nullptr;

  RegSys(const char *_name, SystemCallback _sys, const char *stage_name, const ConstQueryDesc &query_desc) : id(reg_sys_count), sys(_sys), queryDesc(query_desc)
  {
    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;

    if (_name)
      name = ::_strdup(_name);
    if (stage_name)
      stageName = ::_strdup(stage_name);
  }

  RegSys(const char *_name, SystemCallback _sys, const char *stage_name) : RegSys(_name, _sys, stage_name, empty_query_desc)
  {
    mode = Mode::FROM_EXTERNAL_QUERY;
  }

  virtual ~RegSys();

  bool hasCompontent(int id, const char *name) const;
};

enum class ValueType
{
  kEid,
  kStage,
  kEvent,
  kComponent
};

const RegSys *find_sys(const char *name);