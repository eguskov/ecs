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
  typename = typename std::enable_if<has_##method<T>::value>::type \

#define NOT_HAVE_METHOD(T, method) \
  typename = typename std::enable_if<!has_##method<T>::value>::type \

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
  std::string name;
  const RegComp* desc;
};

DEF_HAS_METHOD(require);

struct RegSys
{
  using ExecFunc = void(*)(const RawArg &, const std::vector<CompDesc> &, uint8_t *);
  using StageFunc = void(*)(const RawArg &, const RawArg &, const std::vector<CompDesc> &, uint8_t *);

  char *name = nullptr;

  int id = -1;
  int stageId = -1;
  int eventId = -1;

  const RegSys *next = nullptr;
  ExecFunc execFn = nullptr;
  StageFunc stageFn = nullptr;

  std::bitarray compMask;
  std::vector<CompDesc> components;

  RegSys(const char *_name, int _id);
  virtual ~RegSys();

  virtual void init() = 0;

  bool hasCompontent(int id, const char *name) const;
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
  using Indices = std::make_index_sequence<ArgsCount>;
  using ReturnType = R;

  template <std::size_t N>
  struct Argument
  {
    static_assert(N < ArgsCount, "error: invalid parameter index.");

    using Type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    using PureType = typename std::remove_const<typename std::remove_reference<Type>::type>::type;
    using CompType = RegCompSpec<PureType>;
    using CompDesc = typename CompType::CompDesc;

    constexpr static bool isEid = std::is_same<EntityId, PureType>::value;
    constexpr static bool isStage = std::is_base_of<Stage, PureType>::value;
    constexpr static bool isEvent = std::is_base_of<Event, PureType>::value;
  };

  static SysType sys;
  static std::array<const char*, ArgsCount> componentNames;

  static struct Buffer
  {
    int eid = -1;
    int stage = -1;
    uint8_t mem[256];
  } buffer;

  template<typename T, std::size_t N, std::size_t... I>
  constexpr static auto createNames(std::index_sequence<I...>)
  {
    return std::array<T, N>{ {Argument<I>::CompDesc::name...} };
  }

  constexpr static std::array<const char*, ArgsCount> componentTypeNames = createNames<const char*, ArgsCount>(Indices{});

  template <typename T>
  static inline T toValue(uint8_t *components, int offset)
  {
    if (offset < 0)
      return *(typename std::remove_reference<T>::type *)&buffer.mem[-offset - 1];
    return *(typename std::remove_reference<T>::type *)&components[offset];
  }

  static inline int getCompOffset(bool is_eid, bool is_stage_or_event, const std::vector<CompDesc> &templ_desc, const char *comp_name, int comp_id)
  {
    if (is_eid) return -buffer.eid - 1;
    if (is_stage_or_event) return -buffer.stage - 1;
    for (const auto &d : templ_desc)
      if (d.desc->id == comp_id && d.name == comp_name)
        return d.offset;
    return -1;
  }

  template <size_t... I>
  static inline void execImpl(const std::vector<CompDesc> &templ_desc, uint8_t *components, std::index_sequence<I...>)
  {
    static bool inited = false;
    static std::array<int, ArgsCount> offsets;
    if (!inited)
    {
      offsets = std::array<int, ArgsCount>{ {getCompOffset(Argument<I>::isEid, Argument<I>::isStage || Argument<I>::isEvent, templ_desc, componentNames[I], find_comp(componentTypeNames[I])->id)...} };
    }

    sys(toValue<Args>(components, offsets[I])...);
  }

  static void execEid(const RawArg &eid, const std::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);

    execImpl(templ_desc, components, Indices{});
  }

  static void execStage(const RawArg &stage, const RawArg &eid, const std::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execImpl(templ_desc, components, Indices{});
  }

  RegSysSpec(const char *name, const SysType &_sys) : RegSys(name, reg_sys_count)
  {
    execFn = &execEid;
    stageFn = &execStage;

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

    std::sort(components.begin(), components.end(),
      [](const CompDesc &lhs, const CompDesc &rhs)
      {
        if (lhs.desc->id == rhs.desc->id)
          return lhs.name < rhs.name;
        return lhs.desc->id < rhs.desc->id;
      });

    compMask.resize(reg_comp_count);
    compMask.assign(reg_comp_count, false);

    for (const auto &c : components)
      compMask[c.desc->id] = true;

    if (Argument<0>::isStage)
      stageId = find_comp(Argument<0>::CompDesc::name)->id;
    if (Argument<0>::isEvent)
      eventId = find_comp(Argument<0>::CompDesc::name)->id;
  }
};

const RegSys *find_sys(const char *name);