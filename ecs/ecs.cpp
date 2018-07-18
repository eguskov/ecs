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

struct EntityId
{
  uint32_t handle = 0;
  EntityId(uint32_t h = 0) : handle(h) {}
};

static inline EntityId make_eid(uint16_t gen, uint16_t index)
{
  return EntityId((uint32_t)gen << 16 | index);
}

struct RegComp;

struct Template
{
  struct CompDesc
  {
    int offset;
    const RegComp* desc;
  };

  int size = 0;
  int storageId = -1;

  std::string name;
  std::bitarray compMask;
  std::vector<CompDesc> components;
};

struct RegSys
{
  using ExecFunc = void(*)(EntityId, const std::vector<Template::CompDesc> &, uint8_t *);

  char *name = nullptr;

  int id = -1;
  
  const RegSys *next = nullptr;
  ExecFunc execFn = nullptr;

  std::bitarray compMask;
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

template <> struct Desc<EntityId> { constexpr static char* typeName = "EntityId"; constexpr static char* name = "eid"; };
template <>
struct RegCompSpec<EntityId> : RegComp
{
  using CompType = EntityId;
  using CompDesc = Desc<EntityId>;

  void init(uint8_t *) const override final {}

  RegCompSpec() : RegComp("eid", reg_comp_count, sizeof(CompType))
  {
    next = reg_comp_head;
    reg_comp_head = this;
    ++reg_comp_count;
  }
};
static RegCompSpec<EntityId> _eid;

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
    static inline T toValue(EntityId eid, uint8_t *components, std::size_t offset)
    {
      return *(typename std::remove_reference<T>::type *)&components[offset];
    }
  };

  template <>
  struct Exctract<EntityId>
  {
    static inline EntityId toValue(EntityId eid, uint8_t *, std::size_t)
    {
      return eid;
    }
  };

  static inline size_t getCompOffset(const std::vector<Template::CompDesc> &templ_desc, int comp_id)
  {
    for (const auto &d : templ_desc)
      if (d.desc->id == comp_id)
        return d.offset;
    return -1;
  }

  template <size_t... I>
  static inline void execImpl(EntityId eid, const std::vector<Template::CompDesc> &templ_desc, uint8_t *components, std::index_sequence<I...>)
  {
    static bool inited = false;
    static std::array<size_t, ArgsCount> offsets;
    if (!inited)
      offsets = std::array<size_t, ArgsCount>{ {getCompOffset(templ_desc, find_comp(componentNames[I])->id)...} };

    sys(Exctract<Args>::toValue(eid, components, offsets[I])...);
  }

  static void exec(EntityId eid, const std::vector<Template::CompDesc> &templ_desc, uint8_t *components)
  {
    execImpl(eid, templ_desc, components, Indices{});
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
    std::sort(components.begin(), components.end(),
      [](const RegComp* &lhs, const RegComp* &rhs) { return lhs->id < rhs->id; });
    compMask.resize(reg_comp_count);
    compMask.assign(reg_comp_count, false);

    for (const auto &c : components)
      compMask[c->id] = true;
  }
};

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

struct Storage
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

struct Entity
{
  EntityId eid;
	int templateId = -1;
  int memId = -1;
};

struct System
{
  int id;
  const RegSys *desc;
};

struct EntityManager
{
  std::vector<Template> templates;
  std::vector<Storage> storages;
  std::vector<Entity> entities;
  std::vector<System> systems;

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

    for (const RegSys *sys = reg_sys_head; sys; sys = sys->next)
      systems.push_back({getSystemId(sys->name), sys});

    std::sort(systems.begin(), systems.end(),
      [](const System &lhs, const System &rhs) { return lhs.id > rhs.id; });

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

        templ.compMask[c.desc->id] = true;
      }
      templ.compMask[find_comp("eid")->id] = true;
      templ.size = offset;
    }

    // TODO: Search by size
    storages.emplace_back();
    auto &s = storages.back();
    s.size = templates[0].size;
    templates[0].storageId = storages.size() - 1;
  }

  int getSystemId(const char *name)
  {
    if (::strcmp(name, "update_position")) return 0;
    if (::strcmp(name, "position_printer")) return 1;
    if (::strcmp(name, "test_printer")) return 2;
    return 0;
  }

  EntityId createEntity(const char *templ_name)
  {
    // TOOD: Search template by name
    auto &templ = templates[0];
    auto &storage = storages[templ.storageId];

    uint8_t *mem = storage.allocate();

    for (const auto &c : templ.components)
      c.desc->init(mem + c.offset);

    entities.emplace_back();
    auto &e = entities.back();
    e.eid = make_eid(1, entities.size() - 1);
    e.templateId = 0;
    e.memId = storage.count - 1;

    return e.eid;
  }

  void tick()
  {
    for (auto &e : entities)
    {
      const auto &templ = templates[e.templateId];
      auto &storage = storages[templ.storageId];
      
      for (const auto &sys : systems)
      {
        bool ok = true;
        for (int compId = 0 ; compId < sys.desc->compMask.size(); ++compId)
          if (sys.desc->compMask[compId] && !templ.compMask[compId])
          {
            ok = false;
            break;
          }
        if (!ok)
          continue;
        sys.desc->execFn(e.eid, templ.components, &storage.data[storage.size * e.memId]);
      }
    }
  }
};

void update_position(EntityId eid, const VelocityComponent &velocity, PositionComponent &position)
{
  position.x += velocity.x;
  position.y += velocity.y;
}
REG_SYS(update_position);

void position_printer(EntityId eid, const PositionComponent &position)
{
  std::cout << "(" << position.x << ", " << position.y << ")" << std::endl;
}
REG_SYS(position_printer);

void test_printer(EntityId eid, const TestComponent &test)
{
  std::cout << "TEST (" << test.a << ", " << test.b << ")" << std::endl;
}
REG_SYS(test_printer);

int main()
{
  EntityManager mgr;
  EntityId eid1 = mgr.createEntity("test");
  EntityId eid2 = mgr.createEntity("test");

  mgr.tick();

  return 0;
}

