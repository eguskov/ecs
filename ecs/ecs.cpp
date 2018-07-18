// ecs.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>

namespace std
{
  using bitarray = vector<bool>;
}

#define REG_COMP(type, n) \
  template <> struct Desc<type> { constexpr static char* typeName = #type; constexpr static char* name = #n; };\
  static RegCompSpec<type> _##n(#n); \

#define REG_SYS(func) \
  template <> struct Desc<decltype(func)> { constexpr static char* name = #func; }; \
  static RegSysSpec<decltype(func)> _##func(#func, func); \
  RegSysSpec<decltype(func)>::SysType RegSysSpec<decltype(func)>::sys = &func; \

struct RegComp;

struct Template
{
  struct CompDesc
  {
    int offset;
    const RegComp* desc;
  };

  int size = 0;
  int typeId = -1;

  std::string name;
  std::bitarray compMask;
  std::vector<CompDesc> components;
};

struct RegSys
{
  using ExecFunc = void(*)(const std::vector<Template::CompDesc> &, uint8_t *);

  char *name = nullptr;

  int id = -1;
  
  const RegSys *next = nullptr;
  ExecFunc execFn = nullptr;

  std::vector<const RegComp*> components;

  RegSys(const char *_name, int _id) : id(_id)
  {
    name = ::strdup(_name);
  }

  virtual ~RegSys()
  {
    ::free(name);
  }

  virtual void init() = 0;
};

static RegSys *reg_sys_head = nullptr;
static int reg_sys_count = 0;

static const RegSys *find_sys(const char *name)
{
  const RegSys *head = reg_sys_head;
  while (head)
  {
    if (::strcmp(head->name, name) == 0)
      return head;
    head = head->next;
  }
  return nullptr;
}

struct RegComp
{
  char *name = nullptr;

  int id = -1;
  int size = 0;

  const RegComp *next = nullptr;

  RegComp(const char *_name, int _id, int _size) : id(_id), size(_size)
  {
    name = ::strdup(_name);
  }

  virtual ~RegComp()
  {
    ::free(name);
  }

  virtual void init(uint8_t *mem) const = 0;
};

static RegComp *reg_comp_head = nullptr;
static int reg_comp_count = 0;

static const RegComp *find_comp(const char *name)
{
  const RegComp *head = reg_comp_head;
  while (head)
  {
    if (::strcmp(head->name, name) == 0)
      return head;
    head = head->next;
  }
  return nullptr;
}

template <typename T>
struct Desc;

template <typename T>
struct RegCompSpec : RegComp
{
  using CompType = T;
  using CompDesc = Desc<T>;

  void init(uint8_t *mem) const override final { new (mem) CompType; }

  RegCompSpec(const char *name) : RegComp(name, reg_comp_count, sizeof(CompType))
  {
    next = reg_comp_head;
    reg_comp_head = this;
    ++reg_comp_count;
  }
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

  template<typename T, std::size_t N, std::size_t... I>
  constexpr static auto createDescs(std::index_sequence<I...>)
  {
    return std::array<T, N>{ {ArgumentDesc(Argument<I>::CompDesc::name)...} };
  }

  constexpr static std::array<const char*, ArgsCount> componentNames = createNames<const char*, ArgsCount>(Indices{});

  static SysType sys;

  template <typename T>
  static inline T toValue(uint8_t *components, std::size_t offset)
  {
    return *(typename std::remove_reference<T>::type *)&components[offset];
  }

  static inline size_t getCompOffset(const std::vector<Template::CompDesc> &templ_desc, int comp_id)
  {
    for (const auto &d : templ_desc)
      if (d.desc->id == comp_id)
        return d.offset;
    return -1;
  }

  template <size_t... I>
  static inline void execImpl(const std::vector<Template::CompDesc> &templ_desc, uint8_t *components, std::index_sequence<I...>)
  {
    static bool inited = false;
    static std::array<size_t, ArgsCount> offsets;
    if (!inited)
      offsets = std::array<size_t, ArgsCount>{ {getCompOffset(templ_desc, find_comp(componentNames[I])->id)...} };

    sys(toValue<Args>(components, offsets[I])...);
  }

  static void exec(const std::vector<Template::CompDesc> &templ_desc, uint8_t *components)
  {
    execImpl(templ_desc, components, Indices{});
  }

  RegSysSpec(const char *name, const SysType &_sys) : RegSys(name, reg_sys_count)
  {
    execFn = &exec;
    next = reg_sys_head;
    reg_sys_head = this;
    ++reg_sys_count;
  }

  void init() override final
  {
    for (const char *n : componentNames)
      components.push_back(find_comp(n));
  }
};

struct EntityId
{
  uint32_t handle = 0;
  EntityId(uint32_t h) : handle(h) {}
};

static inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId((uint32_t)gen << 16 | index);
}

struct PositionComponent
{
  float x = 1.f;
  float y = 2.f;
};
REG_COMP(PositionComponent, position);

struct VelocityComponent
{
  float x = 3.f;
  float y = 4.f;
};
REG_COMP(VelocityComponent, velocity);

struct TestComponent
{
  double a = 0;
  int b = 0;
};
REG_COMP(TestComponent, test);

struct Archetype
{
  int count = 0;
  int size = 0;

  std::vector<uint8_t> data;

  uint8_t* allocate()
  {
    ++count;
    data.resize(count * size);
    return &data[(count - 1) * size];
  }
};

struct EntityManager
{
  std::vector<Template> templates;
  std::vector<Archetype> types;

  EntityManager()
  {
    // Init systems' descs
    {
      const RegSys *head = reg_sys_head;
      while (head)
      {
        const_cast<RegSys*>(head)->init();
        head = head->next;
      }
    }

    // Add template
    {
      templates.emplace_back();
      auto &templ = templates.back();
      templ.name = "test";
      templ.compMask.resize(reg_comp_count);
      templ.compMask.assign(reg_comp_count, false);

      const char* compNames[] = { "velocity", "position" };
      for (const auto &name : compNames)
        templ.components.push_back({ 0, find_comp(name) });

      std::sort(templ.components.begin(), templ.components.end(),
        [](const Template::CompDesc &lhs, const Template::CompDesc &rhs) { return lhs.desc->id < rhs.desc->id; });

      int offset = 0;
      for (auto &c : templ.components)
      {
        c.offset = offset;
        offset += c.desc->size;
      }
      templ.size = offset;

      for (const auto &c : templ.components)
        templ.compMask[c.desc->id] = true;
    }

    // TODO: Search by size
    types.emplace_back();
    auto &t = types.back();
    t.size = templates[0].size;
    templates[0].typeId = types.size() - 1;
  }

  EntityId createEntity(const char *templ_name)
  {
    // TOOD: Search template by name
    auto &templ = templates[0];
    auto &type = types[templ.typeId];

    uint8_t *mem = type.allocate();

    for (const auto &c : templ.components)
      c.desc->init(mem + c.offset);

    reg_sys_head->execFn(templ.components, mem);

    return (EntityId)0;
  }

  void tick()
  {
  }
};

void update_position(const VelocityComponent &velocity, PositionComponent &position)
{
  position.x += velocity.x;
  position.y += velocity.y;
}
REG_SYS(update_position);

int main()
{
  EntityManager mgr;
  mgr.createEntity("test");
  // mgr.createEntity("test");

  mgr.tick();

  return 0;
}

