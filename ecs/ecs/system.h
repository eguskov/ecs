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
  using Remap = eastl::fixed_vector<int, 32>;
  using StageFunc = void(RegSys::*)(int count, const EntityId *eids, const RawArg&, Storage**) const;

  char *name = nullptr;

  bool needOrder = true;

  int id = -1;
  int stageId = -1;
  int eventId = -1;

  const RegSys *next = nullptr;

  StageFunc stageFn = nullptr;

  eastl::bitvector<> compMask;
  QueryDesc queryDesc;

  RegSys(const char *_name, int _id, bool need_order = true);
  virtual ~RegSys();

  virtual void init(const EntityManager *mgr) = 0;
  virtual void initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const = 0;

  bool hasCompontent(int id, const char *name) const;
};

enum class ValueType
{
  kEid,
  kStage,
  kEvent,
  kComponent
};

struct ExtraArguments
{
  EntityId eid;
  RawArg stageOrEvent;
};

template <typename T, ValueType ValueType>
struct Value;

template <typename T>
struct Value<T, ValueType::kComponent>
{
  static __forceinline T get(const ExtraArguments &, Storage **storage, int comp_id, int offset)
  {
    return storage[comp_id]->get<typename eastl::remove_reference<T>::type>(offset);
  }
};

template <typename T>
struct Value<T, ValueType::kStage>
{
  static __forceinline T get(const ExtraArguments &args, Storage**, int, int)
  {
    return *(typename eastl::remove_reference<T>::type *)args.stageOrEvent.mem;
  }
};

template <typename T>
struct Value<T, ValueType::kEvent>
{
  static __forceinline T get(const ExtraArguments &args, Storage**, int, int)
  {
    return *(typename eastl::remove_reference<T>::type *)args.stageOrEvent.mem;
  }
};

template <typename T>
struct Value<T, ValueType::kEid>
{
  static __forceinline T get(const ExtraArguments &args, Storage**, int, int)
  {
    return *(typename eastl::remove_reference<T>::type *)&args.eid;
  }
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
  using Indices = eastl::make_index_sequence<ArgsCount>;
  using StringArray = eastl::array<const char*, ArgsCount>;
  using StringList = std::initializer_list<const char*>;
  using StringVector = eastl::vector<eastl::string>;
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

  struct ArgumentDesc
  {
    bool isConst;
    const char *name;
  };

  SysType sys;
  SysFuncType sysFunc;
  StringArray componentNames;
  StringVector haveNames;
  StringVector notHaveNames;
  StringVector isTrueNames;
  StringVector isFalseNames;

  template <std::size_t... I>
  constexpr static auto createNames(eastl::index_sequence<I...>)
  {
    return StringArray{ {Argument<I>::CompDesc::name...} };
  }

  template <std::size_t... I>
  constexpr static auto createArguments(eastl::index_sequence<I...>)
  {
    return eastl::array<ArgumentDesc, ArgsCount>{ { { Argument<I>::isConst, Argument<I>::CompDesc::name }... } };
  }

  constexpr static StringArray componentTypeNames = createNames(Indices{});
  constexpr static eastl::array<ArgumentDesc, ArgsCount> arguments = createArguments(Indices{});

  template <int N>
  struct Counter
  {
    constexpr static int getValue(int acc)
    {
      return (Argument<N>::isComponent ? 0 : 1) + acc + Counter<N - 1>::getValue(acc);
    }
  };

  template <>
  struct Counter<-1>
  {
    constexpr static int getValue(int) { return 0; }
  };

  constexpr static int argumentsOffset = Counter<ArgsCount - 1>::getValue(0);

  template <size_t... I>
  __forceinline void execImpl(const ExtraArguments &args, const int *remap, const int *offsets, Storage **storage, eastl::index_sequence<I...>) const;

  inline void operator()(const EntityVector &eids, const RawArg &stage, Storage **storage) const
  {
    buffer.eid = eid;
    buffer.stage = stage;

    execImpl(remap, offsets, storage, Indices{});
  }

  inline void operator()(const EntityId &eid, const RegSys::Remap &remap, const int *offsets, Storage **storage) const
  {
    buffer.eid = eid;
    execImpl(remap, offsets, storage, Indices{});
  }

  void execEid(const EntityVector &eids, const RegSys::Remap &remap, const int *offsets, Storage **storage) const
  {
    ExtraArguments args;
    args.eid = eid;

    execImpl(args, remap, offsets, storage, Indices{});
  }

  void execStage(int count, const EntityId *eids, const RawArg &stage_or_event, Storage **storage) const
  {
#ifdef ECS_PACK_QUERY_DATA
    const auto &query = g_mgr->queries[id];
    const int *queryData = query.data.data();
#endif

    ExtraArguments args;
    args.stageOrEvent = stage_or_event;

    for (int i = 0; i < count; ++i)
    {
      EntityId eid = eids[i];
      args.eid = eid;
#ifdef ECS_PACK_QUERY_DATA
      const int *remapData = queryData;
      const int offsetCount = queryData[ArgsCount];
      const int *offsets = &queryData[ArgsCount + 1];
      queryData += ArgsCount + offsetCount + 1;
      execImpl(args, remapData, offsets, storage, Indices{});
#else
      const auto &entity = g_mgr->entities[eid.index];
      const auto &templ = g_mgr->templates[entity.templateId];
      const auto &remap = templ.remaps[id];
      execImpl(args, remap.data(), entity.componentOffsets.data(), storage, Indices{});
#endif
    }
  }

  RegSysSpec(const char *name,
    const SysType &_sys,
    const StringArray &names,
    const StringList &have,
    const StringList &not_have,
    const StringList &is_true,
    const StringList &is_false,
    bool need_order) :
    RegSys(name, reg_sys_count, need_order),
    sys(_sys),
    componentNames(names)
  {
    stageFn = (StageFunc)&RegSysSpec::execStage;

    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;

    for (const auto &n : have)
      haveNames.emplace_back(n);
    for (const auto &n : not_have)
      notHaveNames.emplace_back(n);
    for (const auto &n : is_true)
      isTrueNames.emplace_back(n);
    for (const auto &n : is_false)
      isFalseNames.emplace_back(n);

    queryDesc.haveComponents.resize(haveNames.size());
    queryDesc.notHaveComponents.resize(notHaveNames.size());
    queryDesc.isTrueComponents.resize(isTrueNames.size());
    queryDesc.isFalseComponents.resize(isFalseNames.size());
  }

  void init(const EntityManager *mgr) override final
  {
    assert(componentNames.size() == componentTypeNames.size());
    for (size_t i = 0; i < componentTypeNames.size(); ++i)
    {
      const RegComp *desc = find_comp(componentTypeNames[i]);
      assert(desc != nullptr);
      queryDesc.components.push_back({ 0, mgr->getComponentNameId(componentNames[i]), desc });

      if (arguments[i].isConst)
        queryDesc.roComponents.push_back(i);
      else
        queryDesc.rwComponents.push_back(i);
    }

    int index = 0;
    for (const auto &n : haveNames)
    {
      auto &c = queryDesc.haveComponents[index++];
      c.desc = mgr->getComponentDescByName(n.c_str());
      c.nameId = mgr->getComponentNameId(n.c_str());
      assert(c.desc != nullptr);
    }

    index = 0;
    for (const auto &n : notHaveNames)
    {
      auto &c = queryDesc.notHaveComponents[index++];
      c.desc = mgr->getComponentDescByName(n.c_str());
      c.nameId = mgr->getComponentNameId(n.c_str());
      assert(c.desc != nullptr);
    }

    index = 0;
    for (const auto &n : isTrueNames)
    {
      auto &c = queryDesc.isTrueComponents[index++];
      c.desc = mgr->getComponentDescByName(n.c_str());
      c.nameId = mgr->getComponentNameId(n.c_str());
      assert(c.desc != nullptr);
    }

    index = 0;
    for (const auto &n : isFalseNames)
    {
      auto &c = queryDesc.isFalseComponents[index++];
      c.desc = mgr->getComponentDescByName(n.c_str());
      c.nameId = mgr->getComponentNameId(n.c_str());
      assert(c.desc != nullptr);
    }

    compMask.resize(reg_comp_count);
    compMask.set(reg_comp_count, false);

    for (const auto &c : queryDesc.components)
      compMask[c.desc->id] = true;

    if (Argument<0>::isStage)
      stageId = find_comp(Argument<0>::CompDesc::name)->id;
    if (Argument<0>::isEvent)
      eventId = find_comp(Argument<0>::CompDesc::name)->id;
  }

  void initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const override final
  {
    remap.resize(componentNames.size());
    eastl::fill(remap.begin(), remap.end(), -1);

    for (size_t compIdx = 0; compIdx < componentNames.size(); ++compIdx)
    {
      const char *name = componentNames[compIdx];
      const char *typeName = componentTypeNames[compIdx];
      const RegComp *desc = find_comp(typeName);

      for (size_t i = 0; i < template_comps.size(); ++i)
        if (template_comps[i].desc->id == desc->id && template_comps[i].nameId == g_mgr->getComponentNameId(name))
          remap[compIdx] = i;
    }
  }
};

const RegSys *find_sys(const char *name);