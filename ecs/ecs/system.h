#pragma once

#include "stdafx.h"

#include "ecs/stage.h"
#include "ecs/event.h"
#include "ecs/storage.h"
#include "ecs/query.h"

struct EntityManager;

#define __DEF_QUERY(name) struct name { template <typename C> static void exec(C); }

#ifdef __CODEGEN__
#define DEF_SYS(...) __VA_ARGS__ __attribute__((annotate("@system")))
#define DEF_QUERY(name, ...) __DEF_QUERY(name) __VA_ARGS__ __attribute__((annotate("@query")))
#define HAVE_COMP(name) __attribute__((annotate("@have: " #name)))
#define NOT_HAVE_COMP(name) __attribute__((annotate("@not-have: " #name)))
#define IS_TRUE(name) __attribute__((annotate("@is-true: " #name)))
#define IS_FALSE(name) __attribute__((annotate("@is-false: " #name)))
#else
#define DEF_SYS(...)
#define DEF_QUERY(name, ...) __DEF_QUERY(name);
#define HAVE_COMP(...)
#define NOT_HAVE_COMP(...)
#define IS_TRUE(...)
#define IS_FALSE(...)
#endif

#define REG_SYS_BASE(func, ...) \
  template <> struct Desc<decltype(func)> { constexpr static char const* name = #func; }; \
  static RegSysSpec<decltype(func)> _##func(#func, &func, {__VA_ARGS__}); \

#define REG_SYS(func, ...) \
  REG_SYS_BASE(func, __VA_ARGS__) \

#define REG_SYS_1(func, ...) \
  REG_SYS_BASE(func, "", __VA_ARGS__) \

#define REG_SYS_2(func, ...) \
  REG_SYS_BASE(func, "", "", __VA_ARGS__) \

#define DEF_HAS_METHOD(method) \
  template <typename T> \
  struct has_##method \
  { \
    struct has { char d[1]; }; \
    struct notHas { char d[2]; }; \
    template <typename C> static has test(decltype(&C::require)); \
    template <typename C> static notHas test(...); \
    static constexpr bool value = sizeof(test<T>(0)) == sizeof(has); \
  }; \

#define HAS_METHOD(T, method) \
  typename = typename eastl::enable_if<has_##method<T>::value>::type \

#define NOT_HAVE_METHOD(T, method) \
  typename = typename eastl::enable_if<!has_##method<T>::value>::type \

__forceinline int get_offset(int remap, const int *offsets)
{
  return remap >= 0 ? offsets[remap] : -1;
}

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

DEF_HAS_METHOD(require);

struct RegSys
{
  char *name = nullptr;

  int id = -1;
  int stageId = -1;
  int eventId = -1;

  const RegSys *next = nullptr;

  // TODO: Remove compMask
  eastl::bitvector<> compMask;
  ConstQueryDesc queryDesc;

  RegSys(const char *_name, const ConstQueryDesc &query_desc, int _id);
  virtual ~RegSys();

  virtual void init(const EntityManager *mgr) = 0;

  bool hasCompontent(int id, const char *name) const;

  virtual void execImpl(const RawArg &stage_or_event, Query &query) const = 0;

  __forceinline void exec(Query &query, const RawArg &stage_or_event) const
  {
    execImpl(stage_or_event, query);
  }
};

enum class ValueType
{
  kEid,
  kStage,
  kEvent,
  kComponent
};

template <size_t D, typename T>
struct RegSysSpec;

template<size_t D, class R, class... Args>
struct RegSysSpec<D, R(*)(Args...)> : RegSysSpec<D, R(Args...)> {};

template<size_t D, class R, class... Args>
struct RegSysSpec<D, R(Args...)> : RegSys
{
  static constexpr std::size_t Hash = D;
  static constexpr std::size_t ArgsCount = sizeof...(Args);

  using SysType = R(*)(Args...);
  using SysFuncType = eastl::function<R(Args...)>;
  using ReturnType = R;

  template <std::size_t N>
  struct Argument
  {
    static_assert(N < ArgsCount, "error: invalid parameter index.");

    using Type = typename eastl::tuple_element<N, eastl::tuple<Args...>>::type;
    using PureType = typename eastl::remove_const<typename eastl::remove_reference<Type>::type>::type;
    using CompType = RegCompSpec<PureType>;
    using CompDesc = typename CompType::CompDesc;

    constexpr static bool isEid = eastl::is_same<EntityId, PureType>::value;
    constexpr static bool isStage = eastl::is_base_of<Stage, PureType>::value;
    constexpr static bool isEvent = eastl::is_base_of<Event, PureType>::value;
    constexpr static bool isComponent = !isEid && !isStage && !isEvent;
    constexpr static bool isConst = !eastl::is_reference<Type>::value || eastl::is_const<typename eastl::remove_reference<Type>::type>::value;

    constexpr static ValueType getValueType()
    {
      return
        isEid ? ValueType::kEid :
        isStage ? ValueType::kStage :
        isEvent ? ValueType::kEvent :
        ValueType::kComponent;
    }

    constexpr static ValueType valueType = getValueType();
  };

  SysFuncType sysFunc;

  RegSysSpec(const char *name, const ConstQueryDesc &query_desc) : RegSys(name, query_desc, reg_sys_count)
  {
    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;
  }

  void init(const EntityManager *mgr) override final
  {
    if (Argument<0>::isStage)
      stageId = find_comp(Argument<0>::CompDesc::name)->id;
    if (Argument<0>::isEvent)
      eventId = find_comp(Argument<0>::CompDesc::name)->id;
  }

  virtual void execImpl(const RawArg &stage_or_event, Query &query) const override final;
};

const RegSys *find_sys(const char *name);