#pragma once

#include "stdafx.h"

#include "ecs/event.h"
#include "ecs/query.h"

struct EntityManager;

#define ECS_FOR_EACH_1(WHAT, X) WHAT(X)
#define ECS_FOR_EACH_2(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_1(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_3(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_2(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_4(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_3(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_5(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_4(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_6(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_5(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_7(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_6(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_8(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_7(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_9(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_8(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_10(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_9(WHAT, __VA_ARGS__)
#define ECS_FOR_EACH_11(WHAT, X, ...) WHAT(X)ECS_FOR_EACH_10(WHAT, __VA_ARGS__)
//... repeat as needed

#define ECS_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,NAME,...) NAME
#define ECS_FOR_EACH(action,...) \
  ECS_GET_MACRO(__VA_ARGS__,ECS_FOR_EACH_11,ECS_FOR_EACH_10,ECS_FOR_EACH_9,ECS_FOR_EACH_8,ECS_FOR_EACH_7,ECS_FOR_EACH_6,ECS_FOR_EACH_5,\
                ECS_FOR_EACH_4,ECS_FOR_EACH_3,ECS_FOR_EACH_2,ECS_FOR_EACH_1)(action,__VA_ARGS__)

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
  #define ECS_LAZY_QUERY struct ecs_lazy_query {};
  #define ECS_SYSTEM struct ecs_system {};
  #define ECS_SYSTEM_IN_JOBS struct ecs_system_in_jobs {};
  #define ECS_JOBS_CHUNK_SIZE(n) struct ecs_jobs_chunk_size { static constexpr char const *ql_expr = #n; };

  #define ECS_BARRIER struct ecs_barrier {};
  #define ECS_BEFORE(...) struct ecs_before { static constexpr char const *ql_expr = #__VA_ARGS__; };
  #define ECS_AFTER(...)  struct ecs_after  { static constexpr char const *ql_expr = #__VA_ARGS__; };

  #define ECS_BIND(expr) __attribute__(( annotate( "@bind: " #expr ) ))
  #define ECS_BIND_TYPE(module, expr) struct ecs_bind_type { static constexpr char const *ql_expr = #module ";" #expr; };

  #define ECS_BIND_FIELD(x) struct ecs_bind_field_##x { static constexpr char const *ql_expr = #x; };
  #define ECS_BIND_FIELDS(...) struct ecs_bind { ECS_FOR_EACH(ECS_BIND_FIELD, __VA_ARGS__) };

  #define ECS_BIND_ALL_FIELDS struct ecs_bind_all_fields {};
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
    static __forceinline Self get(QueryIterator &iter);\
    static __forceinline Index* index();\

  #define ECS_LAZY_QUERY\
    template <typename Callable> static __forceinline void perform(Callable);\

  #define ECS_SYSTEM
  #define ECS_SYSTEM_IN_JOBS
  #define ECS_JOBS_CHUNK_SIZE(...)

  #define ECS_BARRIER
  #define ECS_BEFORE(...)
  #define ECS_AFTER(...)

  #define ECS_BIND(...)
  #define ECS_BIND_TYPE(module, expr)
  #define ECS_BIND_FIELDS(...)
  #define ECS_BIND_ALL_FIELDS

  #define ECS_BIND_POD(module) ECS_BIND_TYPE(module, isLocal=true); ECS_BIND_ALL_FIELDS;
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

#define GET_COMPONENT_VALUE(c, T) T c = type.get<T>(entity_idx, type.getComponentIndex(HASH(#c)))
#define GET_COMPONENT_VALUE_ITER(c, t) auto it_##c = query.chunks[compIdx_##c + chunkIdx * query.componentsCount].begin<t>()
#define GET_COMPONENT_ITER(q, c, t) auto c = query.iter<t>(index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components))
#define GET_COMPONENT_INDEX(q, c) static constexpr int compIdx_##c = index_of_component<_countof(q##_components)>::get(HASH(#c), q##_components)
#define GET_COMPONENT(q, i, t, c) i.get<t>(INDEX_OF_COMPONENT(q, c))

struct SystemId : Handle_8_24
{
  SystemId(uint32_t h = 0) : Handle_8_24(h) {}
  explicit SystemId(uint32_t gen, uint32_t idx) : Handle_8_24(gen, idx) {}
};

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

struct SystemDescription final
{
  enum class Mode { FROM_INTERNAL_QUERY, FROM_EXTERNAL_QUERY };
  using SystemCallback = void (*)(const RawArg &stage_or_event, Query&);

  HashedString name;
  HashedString stageName;

  int id = -1;

  Mode mode = Mode::FROM_INTERNAL_QUERY;

  filter_t filter;

  static const SystemDescription *head;
  static int count;

  const SystemDescription *next = nullptr;

  ConstQueryDescription queryDesc;
  SystemCallback sys = nullptr;

  eastl::string before;
  eastl::string after;

  bool isDynamic = false;

  SystemDescription(const HashedString &_name, SystemCallback _sys, const HashedString &stage_name, const ConstQueryDescription &query_desc, const char *_before, const char *_after, filter_t &&f = nullptr):
    name(_name),
    stageName(stage_name),
    id(SystemDescription::count),
    sys(_sys),
    queryDesc(query_desc),
    before(_before),
    after(_after),
    filter(eastl::move(f))
  {
    next = SystemDescription::head;
    SystemDescription::head = this;
    ++SystemDescription::count;
  }

  SystemDescription(const HashedString &_name, SystemCallback _sys, const HashedString &stage_name, const char *_before, const char *_after):
    SystemDescription(_name, _sys, stage_name, empty_query_desc, _before, _after)
  {
    mode = Mode::FROM_EXTERNAL_QUERY;
  }

  SystemDescription(const HashedString &_name, SystemCallback _sys, const HashedString &stage_name, const ConstQueryDescription &query_desc, const char *_before, const char *_after, Mode _mode):
    SystemDescription(_name, _sys, stage_name, query_desc, _before, _after)
  {
    mode = _mode;
  }

  SystemDescription(const HashedString &_name, const char *_before, const char *_after):
    SystemDescription(_name, nullptr, HASH(""), empty_query_desc, _before, _after)
  {
  }

  bool hasCompontent(int id, const char *name) const;
};

const SystemDescription *find_system(const HashedString &name);