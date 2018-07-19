#pragma once

#include "stdafx.h"

#define REG_SYS(func) \
  template <> struct Desc<decltype(func)> { constexpr static char* name = #func; }; \
  static RegSysSpec<decltype(func)> _##func(#func, func); \
  RegSysSpec<decltype(func)>::SysType RegSysSpec<decltype(func)>::sys = &func; \
  RegSysSpec<decltype(func)>::Buffer RegSysSpec<decltype(func)>::buffer; \

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
  const RegComp* desc;
};

struct RegSys
{
  using ExecFunc = void(*)(const RawArg &, const std::vector<CompDesc> &, uint8_t *);
  using StageFunc = void(*)(const RawArg &, const RawArg &, const std::vector<CompDesc> &, uint8_t *);

  char *name = nullptr;

  int id = -1;
  int stageId = -1;

  const RegSys *next = nullptr;
  ExecFunc execFn = nullptr;
  StageFunc stageFn = nullptr;

  std::bitarray compMask;
  std::vector<const RegComp*> components;

  RegSys(const char *_name, int _id);
  virtual ~RegSys();

  virtual void init() = 0;
};

template <typename T>
struct RegSysSpec;

template<class R, class... Args>
struct RegSysSpec<R(*)(Args...)> : RegSysSpec<R(Args...)> {};

template<class R, class... Args>
struct RegSysSpec<R(Args...)> : RegSys
{
  using SysType = R(*)(Args...);

  static constexpr std::size_t ArgsCount = sizeof...(Args);
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
  };

  template<typename T, std::size_t N, std::size_t... I>
  constexpr static auto createNames(std::index_sequence<I...>)
  {
    return std::array<T, N>{ {Argument<I>::CompDesc::name...} };
  }

  constexpr static std::array<const char*, ArgsCount> componentNames = createNames<const char*, ArgsCount>(Indices{});

  static SysType sys;

  template <typename T>
  struct Exctract
  {
    static inline T toValue(uint8_t *components, int offset)
    {
      if (offset < 0)
        return *(typename std::remove_reference<T>::type *)&buffer.mem[-offset - 1];
      return *(typename std::remove_reference<T>::type *)&components[offset];
    }
  };

  static inline int getCompOffset(bool is_eid, bool is_stage, const std::vector<CompDesc> &templ_desc, int comp_id)
  {
    for (const auto &d : templ_desc)
      if (d.desc->id == comp_id)
        return d.offset;
    if (is_eid) return -buffer.eid - 1;
    if (is_stage) return -buffer.stage - 1;
    return -1;
  }

  template <size_t... I>
  static inline void execImpl(EntityId eid, const std::vector<CompDesc> &templ_desc, uint8_t *components, std::index_sequence<I...>)
  {
    static bool inited = false;
    static std::array<size_t, ArgsCount> offsets;
    if (!inited)
      offsets = std::array<size_t, ArgsCount>{ {getCompOffset(templ_desc, find_comp(componentNames[I])->id)...} };

    sys(Exctract<Args>::toValue(eid, components, offsets[I])...);
  }

  static void exec(EntityId eid, const std::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    execImpl(eid, templ_desc, components, Indices{});
  }

  template <size_t... I>
  static inline void execStageImpl(const std::vector<CompDesc> &templ_desc, uint8_t *components, std::index_sequence<I...>)
  {
    static bool inited = false;
    static std::array<int, ArgsCount> offsets;
    if (!inited)
      offsets = std::array<int, ArgsCount>{ {getCompOffset(std::is_same<EntityId, Argument<I>::Type>::value, std::is_base_of<Stage, Argument<I>::PureType>::value, templ_desc, find_comp(componentNames[I])->id)...} };

    sys(Exctract<Args>::toValue(components, offsets[I])...);
  }

  static struct Buffer
  {
    int eid = -1;
    int stage = -1;
    uint8_t mem[256];
  } buffer;

  static void execStage(const RawArg &eid, const std::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);

    execStageImpl(templ_desc, components, Indices{});
  }

  static void execStage1(const RawArg &stage, const RawArg &eid, const std::vector<CompDesc> &templ_desc, uint8_t *components)
  {
    buffer.eid = 0;
    buffer.stage = eid.size;
    ::memcpy(&buffer.mem[buffer.eid], eid.mem, eid.size);
    ::memcpy(&buffer.mem[buffer.stage], stage.mem, stage.size);

    execStageImpl(templ_desc, components, Indices{});
  }

  RegSysSpec(const char *name, const SysType &_sys) : RegSys(name, reg_sys_count)
  {
    RawArgSpec<sizeof(EntityId)> eid;

    // execFn = &exec;
    execFn = &execStage;
    stageFn = &execStage1;
    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;
  }

  void init() override final
  {
    for (const char *n : componentNames)
      components.push_back(find_comp(n));
    std::sort(components.begin(), components.end(),
      [](const RegComp* &lhs, const RegComp* &rhs) { return lhs->id < rhs->id; });
    compMask.resize(reg_comp_count);
    compMask.assign(reg_comp_count, false);

    for (const auto &c : components)
      compMask[c->id] = true;

    if (std::is_base_of<Stage, Argument<0>::PureType>::value)
      stageId = find_comp(Argument<0>::CompDesc::name)->id;
  }
};

const RegSys *find_sys(const char *name);