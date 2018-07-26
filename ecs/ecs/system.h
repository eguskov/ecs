#pragma once

#include "stdafx.h"

#define REG_SYS_BASE(func) \
  template <> struct Desc<decltype(func)> { constexpr static char* name = #func; }; \
  static RegSysSpec<decltype(func)> _##func(#func, func); \
  RegSysSpec<decltype(func)>::SysType RegSysSpec<decltype(func)>::sys = &func; \
  RegSysSpec<decltype(func)>::Buffer RegSysSpec<decltype(func)>::buffer; \

#define REG_SYS(func, ...) \
  REG_SYS_BASE(func) \
  decltype(RegSysSpec<decltype(func)>::componentNames) RegSysSpec<decltype(func)>::componentNames = {__VA_ARGS__};\

#define REG_SYS_1(func, ...) \
  REG_SYS_BASE(func) \
  decltype(RegSysSpec<decltype(func)>::componentNames) RegSysSpec<decltype(func)>::componentNames = {"", __VA_ARGS__};\

#define REG_SYS_2(func, ...) \
  REG_SYS_BASE(func) \
  decltype(RegSysSpec<decltype(func)>::componentNames) RegSysSpec<decltype(func)>::componentNames = {"", "", __VA_ARGS__};\

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
  uint8_t buffer[Size];
  RawArgSpec() : RawArg(Size, buffer) {}
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
  using StageFuncSoA = void(*)(const RawArg &, const RawArg &, const RegSys::Remap &, const int *, Storage *);

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
  using Indices = eastl::make_index_sequence<ArgsCount>;
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

  static SysType sys;
  static eastl::array<const char*, ArgsCount> componentNames;

  static struct Buffer
  {
    int eid = -1;
    int stage = -1;
    uint8_t mem[256];
  } buffer;

  template<typename T, std::size_t N, std::size_t... I>
  constexpr static auto createNames(eastl::index_sequence<I...>)
  {
    return eastl::array<T, N>{ {Argument<I>::CompDesc::name...} };
  }

  constexpr static eastl::array<const char*, ArgsCount> componentTypeNames = createNames<const char*, ArgsCount>(Indices{});

  template <typename T>
  static inline T toValue(uint8_t *components, int offset)
  {
    if (offset < 0)
      return *(typename eastl::remove_reference<T>::type *)&buffer.mem[-offset - 1];
    return *(typename eastl::remove_reference<T>::type *)&components[offset];
  }

  static inline int getCompOffset(bool is_eid, bool is_stage_or_event, const eastl::vector<CompDesc> &templ_desc, const char *comp_name, int comp_id)
  {
    if (is_eid) return -buffer.eid - 1;
    if (is_stage_or_event) return -buffer.stage - 1;
    for (const auto &d : templ_desc)
      if (d.desc->id == comp_id && d.name == comp_name)
        return d.offset;
    return -1;
  }

  template <size_t... I>
  static inline void execImpl(const eastl::vector<CompDesc> &templ_desc, uint8_t *components, eastl::index_sequence<I...>)
  {
    static bool inited = false;
    static eastl::array<int, ArgsCount> offsets;
    if (!inited)
    {
      offsets = eastl::array<int, ArgsCount>{ {getCompOffset(Argument<I>::isEid, Argument<I>::isStage || Argument<I>::isEvent, templ_desc, componentNames[I], find_comp(componentTypeNames[I])->id)...} };
    }

    sys(toValue<Args>(components, offsets[I])...);
  }

  static void execEid(const RawArg &eid, const eastl::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);

    execImpl(templ_desc, components, Indices{});
  }

  static void execStage(const RawArg &stage, const RawArg &eid, const eastl::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execImpl(templ_desc, components, Indices{});
  }

  template <typename T, ValueSoAType ValueType>
  struct ValueSoA;

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kComponent>
  {
    static inline T get(Storage *components, int comp_id, int offset)
    {
      return *(typename eastl::remove_reference<T>::type *)&components[comp_id][offset];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kStage>
  {
    static inline T get(Storage*, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buffer.mem[buffer.stage];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kEvent>
  {
    static inline T get(Storage*, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buffer.mem[buffer.stage];
    }
  };

  template <typename T>
  struct ValueSoA<T, ValueSoAType::kEid>
  {
    static inline T get(Storage *, int, int)
    {
      return *(typename eastl::remove_reference<T>::type *)&buffer.mem[buffer.eid];
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

  constexpr static int fixArgumentsOffset(int i)
  {
    return i < argumentsOffset ? 0 : i - argumentsOffset;
  }

  static inline int getCompOffsetIndexSoA(const eastl::vector<CompDesc> &templ_desc, const char *comp_name, int comp_id)
  {
    for (size_t i = 0; i < templ_desc.size(); ++i)
      if (templ_desc[i].desc->id == comp_id && templ_desc[i].name == comp_name)
        return i;
    return -1;
  }

  static inline int getOffset(int i, const RegSys::Remap &remap, const int *offsets)
  {
    return remap[i] >= 0 ? offsets[remap[i]] : -1;
  }

  template <size_t... I>
  static inline void execImplSoA(const RegSys::Remap &remap, const int *offsets, Storage *components, eastl::index_sequence<I...>)
  {
    sys(ValueSoA<Args, Argument<I>::valueType>::get(
      components,
      RegCompSpec<Argument<I>::PureType>::ID,
      getOffset(I, remap, offsets))...);
  }

  static void execStageSoA(const RawArg &stage, const RawArg &eid, const RegSys::Remap &remap, const int *offsets, Storage *components)
  {
    // Call in loop for eids
    // This must be inlined !!!
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execImplSoA(remap, offsets, components, Indices{});
  }

  RegSysSpec(const char *name, const SysType &_sys) : RegSys(name, reg_sys_count)
  {
    execFn = &execEid;
    stageFn = &execStage;
    stageFnSoA = &execStageSoA;

    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;
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