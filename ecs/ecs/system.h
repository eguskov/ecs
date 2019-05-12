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

  struct ql_index_by_component {};
  #define QL_INDEX_BY_COMPONENT(x) x;

  #define QL_HAVE(...) struct ql_have { QL_FOREACH(QL_COMPONENT, __VA_ARGS__) };
  #define QL_NOT_HAVE(...) struct ql_not_have { QL_FOREACH(QL_COMPONENT, __VA_ARGS__) };
  #define QL_WHERE(expr) struct ql_where { static constexpr char const *ql_expr = #expr; };
  #define QL_JOIN(expr) struct ql_join { static constexpr char const *ql_expr = #expr; };
  #define QL_INDEX(...) struct ql_index { QL_FOREACH(QL_INDEX_BY_COMPONENT, __VA_ARGS__) };
  #define QL_INDEX_LOOKUP(expr) struct ql_index_lookup { static constexpr char const *ql_expr = #expr; };

  #define ECS_QUERY struct ecs_query {};
  #define ECS_SYSTEM struct ecs_system {};
  #define ECS_SYSTEM_IN_JOBS struct ecs_system_in_jobs {};
  #define ECS_JOBS_CHUNK_SIZE(n) struct ecs_jobs_chunk_size { static constexpr char const *ql_expr = #n; };
#else
  #define QL_HAVE(...)
  #define QL_NOT_HAVE(...)
  #define QL_WHERE(...)
  #define QL_JOIN(...)
  #define QL_INDEX(...)
  #define QL_INDEX_LOOKUP(...)

  #define ECS_QUERY\
    template <typename Callable> static __forceinline void foreach(Callable);\
    static __forceinline int count();\
    static auto detect_self_helper() -> std::remove_reference<decltype(*this)>::type;\
    using Self = decltype(detect_self_helper());\
    static __forceinline Self get(Query::AllIterator &iter);\
    static __forceinline Index* index();\

  #define ECS_SYSTEM
  #define ECS_SYSTEM_IN_JOBS
  #define ECS_JOBS_CHUNK_SIZE(...)
#endif

#ifdef _DEBUG
#define ECS_RUN ECS_SYSTEM; static void run
#define ECS_RUN_IN_JOBS ECS_SYSTEM_IN_JOBS; static void run
#define ECS_RUN_T ECS_SYSTEM; template <typename _> static void run
#define ECS_ADD_JOBS static jobmanager::JobId addJobs
#else
#define ECS_RUN ECS_SYSTEM; static __forceinline void run
#define ECS_RUN_IN_JOBS ECS_SYSTEM_IN_JOBS; static __forceinline void run
#define ECS_RUN_T ECS_SYSTEM; template <typename _> static __forceinline void run
#define ECS_ADD_JOBS static __forceinline jobmanager::JobId addJobs
#endif

#define INDEX_OF_COMPONENT(query, component) eastl::integral_constant<int, index_of_component<_countof(query##_components)>::get(HASH(#component), query##_components)>::value

#define GET_COMPONENT_VALUE(c, t) t c = type.storages[type.getComponentIndex(HASH(#c))]->getByIndex<t>(entity_idx)
#define GET_COMPONENT_VALUE_ITER(c, t) auto it_##c = query.chunks[compIdx_##c + chunkIdx * query.componentsCount].begin<t>()
#define GET_COMPONENT_ITER(q, c, t) auto c = query.iter<t>(index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components))
#define GET_COMPONENT_INDEX(q, c) static constexpr int compIdx_##c = index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components)
#define GET_COMPONENT(q, i, t, c) i.get<t>(INDEX_OF_COMPONENT(q, c))

struct SystemDescription;
extern SystemDescription *reg_sys_head;
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

struct SystemDescription
{
  enum class Mode { FROM_INTERNAL_QUERY, FROM_EXTERNAL_QUERY };
  using SystemCallback = void (*)(const RawArg &stage_or_event, Query&);

  ConstHashedString name;
  char *stageName = nullptr;

  int id = -1;
  int stageId = -1;

  Mode mode = Mode::FROM_INTERNAL_QUERY;

  filter_t filter;

  const SystemDescription *next = nullptr;

  ConstQueryDescription queryDesc;
  SystemCallback sys = nullptr;

  SystemDescription(const ConstHashedString &_name, SystemCallback _sys, const char *stage_name, const ConstQueryDescription &query_desc, filter_t &&f = nullptr) : name(_name), id(reg_sys_count), sys(_sys), queryDesc(query_desc), filter(eastl::move(f))
  {
    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;

    if (stage_name)
      stageName = ::_strdup(stage_name);
  }

  SystemDescription(const ConstHashedString &_name, SystemCallback _sys, const char *stage_name) : SystemDescription(_name, _sys, stage_name, empty_query_desc)
  {
    mode = Mode::FROM_EXTERNAL_QUERY;
  }

  SystemDescription(const ConstHashedString &_name, SystemCallback _sys, const char *stage_name, const ConstQueryDescription &query_desc, Mode _mode) : SystemDescription(_name, _sys, stage_name, query_desc)
  {
    mode = _mode;
  }

  virtual ~SystemDescription();

  bool hasCompontent(int id, const char *name) const;
};

enum class ValueType
{
  kEid,
  kStage,
  kEvent,
  kComponent
};

const SystemDescription *find_system(const ConstHashedString &name);