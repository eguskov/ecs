#pragma once

#include "stdafx.h"

#define REG_SYS_BASE(func, ...) \
  template <> struct Desc<decltype(func)> { constexpr static char* name = #func; }; \
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

struct CompDesc
{
  int offset;
  eastl::string name;
  const RegComp* desc;
};

struct Storage
{
  int count = 0;
  int size = 0;

  eastl::vector<uint8_t> data;

  uint8_t* allocate()
  {
    ++count;
    data.resize(count * size);
    return &data[(count - 1) * size];
  }

  operator uint8_t*()
  {
    return &data[0];
  }
};

DEF_HAS_METHOD(require);

struct RegSys
{
  using Remap = eastl::fixed_vector<int, 32>;

  using ExecFunc = void(*)(const RawArg &, const eastl::vector<CompDesc> &, uint8_t *);
  using StageFunc = void(*)(const RawArg &, const RawArg &, const eastl::vector<CompDesc> &, uint8_t *);
  using StageFuncSoA = void(RegSys::*)(const RawArg &, const RawArg &, const RegSys::Remap &, const int *, Storage *) const;

  char *name = nullptr;

  int id = -1;
  int stageId = -1;
  int eventId = -1;

  const RegSys *next = nullptr;
  ExecFunc execFn = nullptr;
  StageFunc stageFn = nullptr;
  StageFuncSoA stageFnSoA = nullptr;

  eastl::bitvector<> compMask;
  eastl::vector<CompDesc> components;

  RegSys(const char *_name, int _id);
  virtual ~RegSys();

  virtual void init() = 0;
  virtual void initRemap(const eastl::vector<CompDesc> &template_comps, Remap &remap) const = 0;

  bool hasCompontent(int id, const char *name) const;
};

struct CompArrayDesc
{
  const RegSys::Remap &remap;
  const int *offsets;
  int count;
  int remapOffset;

  inline int offset(int comp_idx) const { offsets[remap[remapOffset + comp_idx]]; };
};

enum class ValueSoAType
{
  kEid,
  kStage,
  kEvent,
  kComponent
};

template <typename T>
struct RegSysSpec;

template<class R, class... Args>
struct RegSysSpec<R(*)(Args...)> : RegSysSpec<R(Args...)> {};

template<class R, class... Args>
struct RegSysSpec<R(Args...)> : RegSys
{
  static constexpr std::size_t ArgsCount = sizeof...(Args);

  using SysType = R(*)(Args...);
  using SysFuncType = eastl::function<R(Args...)>;
  using Indices = eastl::make_index_sequence<ArgsCount>;
  using StringArray = eastl::array<const char*, ArgsCount>;
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

    constexpr static ValueSoAType getValueType()
    {
      return
        isEid ? ValueSoAType::kEid :
        isStage ? ValueSoAType::kStage :
        isEvent ? ValueSoAType::kEvent :
        ValueSoAType::kComponent;
    }

    constexpr static ValueSoAType valueType = getValueType();
  };

  SysType sys;
  SysFuncType sysFunc;
  StringArray componentNames;

  mutable struct Buffer
  {
    int eid = -1;
    int stage = -1;
    eastl::array<uint8_t, 256> mem;
  } buffer;

  template <std::size_t... I>
  constexpr static auto createNames(eastl::index_sequence<I...>)
  {
    return StringArray{ {Argument<I>::CompDesc::name...} };
  }

  constexpr static StringArray componentTypeNames = createNames(Indices{});

  template <typename T, ValueSoAType ValueType>
  struct ValueSoA;

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kComponent>
  {
    static inline T get(const Buffer &buf, Storage *components, int comp_id, int offset)
    {
      return *(typename eastl::remove_reference<T>::type *)&components[comp_id][offset];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kStage>
  {
    static inline T get(const Buffer &buf, Storage*, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buf.mem[buf.stage];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kEvent>
  {
    static inline T get(const Buffer &buf, Storage*, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buf.mem[buf.stage];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kEid>
  {
    static inline T get(const Buffer &buf, Storage *, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buf.mem[buf.eid];
    }
  };

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

  template <typename T, size_t... I>
  inline void execImplSoA(const T &_sys, const RegSys::Remap &remap, const int *offsets, Storage *components, eastl::index_sequence<I...>) const
  {
    //CompArrayDesc d{ remap, offsets, 10 /* Entities count */, argumentsOffset };

    _sys(ValueSoA<Args, Argument<I>::valueType>::get(
      buffer,
      components,
      RegCompSpec<Argument<I>::PureType>::ID,
      remap[I] >= 0 ? offsets[remap[I]] : -1)...);
  }

  inline void operator()(const RawArg &stage, const RawArg &eid, const RegSys::Remap &remap, const int *offsets, Storage *components) const
  {
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execImplSoA(sysFunc, remap, offsets, components, Indices{});
  }

  inline void operator()(const RawArg &eid, const RegSys::Remap &remap, const int *offsets, Storage *components) const
  {
    buffer.eid = 0;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);

    execImplSoA(sysFunc, remap, offsets, components, Indices{});
  }

  void execStageSoA(const RawArg &stage, const RawArg &eid, const RegSys::Remap &remap, const int *offsets, Storage *components) const
  {
    // Call in loop for eids
    // This must be inlined !!!
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execImplSoA(sys, remap, offsets, components, Indices{});
  }

  void execEidSoA(const RawArg &eid, const RegSys::Remap &remap, const int *offsets, Storage *components) const
  {
    buffer.eid = 0;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);

    execImplSoA(sys, remap, offsets, components, Indices{});
  }

  RegSysSpec(const char *name, const SysType &_sys, const StringArray &arr) : RegSys(name, reg_sys_count), sys(_sys), componentNames(arr)
  {
    stageFnSoA = (StageFuncSoA)&RegSysSpec::execStageSoA;

    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;
  }

  RegSysSpec(SysFuncType &&_sys, StringArray &&arr) : RegSys(nullptr, -1), sysFunc(eastl::move(_sys)), componentNames(eastl::move(arr))
  {
  }

  void init() override final
  {
    assert(componentNames.size() == componentTypeNames.size());
    for (size_t i = 0; i < componentTypeNames.size(); ++i)
    {
      const RegComp *desc = find_comp(componentTypeNames[i]);
      assert(desc != nullptr);
      components.push_back({ 0, componentNames[i], desc });
    }

    eastl::sort(components.begin(), components.end(),
      [](const CompDesc &lhs, const CompDesc &rhs)
      {
        if (lhs.desc->id == rhs.desc->id)
          return lhs.name < rhs.name;
        return lhs.desc->id < rhs.desc->id;
      });

    compMask.resize(reg_comp_count);
    compMask.set(reg_comp_count, false);

    for (const auto &c : components)
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
        if (template_comps[i].desc->id == desc->id && template_comps[i].name == name)
          remap[compIdx] = i;
    }
  }
};

const RegSys *find_sys(const char *name);