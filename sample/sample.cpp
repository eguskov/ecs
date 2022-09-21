#include <random>
#include <ctime>

#include <sys/stat.h>

#include <ecs/ecs.h>
#include <ecs/hash.h>
#include <ecs/perf.h>
#include <ecs/jobmanager.h>
#include <ecs/autoBind.h>

#include <raylib.h>

#pragma warning(push)
#pragma warning(disable: 4838)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#pragma warning(pop)

#include <daScript/daScript.h>
#include <daScript/simulate/fs_file_info.h>
#include <daScript/misc/sysos.h>

#include "update.h"
#include "boids.h"

PULL_ESC_CORE;

Camera2D camera;
int screen_width = 800;
int screen_height = 600;

std::string get_hashed_string(const HashedString &s)
{
  return std::string(s.str);
}

#include <EASTL/unique_ptr.h>

struct Texture2DAnnotation : das::ManagedStructureAnnotation<Texture2D, false>
{
  Texture2DAnnotation(das::ModuleLibrary &ml) : das::ManagedStructureAnnotation<Texture2D, false>("Texture2D", ml)
  {
    cppName = " ::Texture2D";
  }
};

ECS_COMPONENT_TYPE_DETAILS(Texture2D);

struct RaylibModule final : public das::Module
{
  RaylibModule() : das::Module("raylib")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(this);

    addAnnotation(das::make_smart<Texture2DAnnotation>(lib));

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(RaylibModule);

struct AnimModule final : public das::Module
{
  AnimModule() : das::Module("anim")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("anim"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(AnimModule);

struct RenderModule final : public das::Module
{
  RenderModule() : das::Module("render")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("render"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(RenderModule);

struct PhysModule final : public das::Module
{
  PhysModule() : das::Module("phys")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("phys"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(PhysModule);

struct SampleModule final : public das::Module
{
  SampleModule() : das::Module("sample")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(das::Module::require("ecs"));
    lib.addModule(this);

    do_auto_bind_module(HASH("sample"), *this, lib);

    verifyAotReady();
  }

  das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(SampleModule);

using DasContextPtr = eastl::unique_ptr<EcsContext>;
DasContextPtr dasCtx;

static constexpr char *dasRoot = "../libs/daScript";
static constexpr char *dasProject = "scripts/project.das_project";
static constexpr char *dasInitScript = "scripts/sample.das";
static constexpr char *dasMainScript = "scripts/main.das";
static eastl::hash_map<eastl::string, SystemId> dasMainSystems;
static eastl::vector<QueryId> dasMainQueries;
static time_t dasMainLastModified = 0;
static bool isDasInitScriptLoaded = false;

template <typename TFileAccess>
static bool compile_and_simulate_script(DasContextPtr &ctx, const char *path, das::ModuleGroup &libGroup, const TFileAccess &file_access)
{
  das::TextPrinter tout;
  auto program = das::compileDaScript(path, file_access, tout, libGroup);
  if (!program)
    return false;
  
  if (program->failed())
  {
    for (auto &err : program->errors)
      tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
    std::cout.flush();
    return false;
  }

  DasContextPtr tmpCtx = eastl::make_unique<EcsContext>(program->getContextStackSize());
    
  if (!program->simulate(*tmpCtx, tout))
  {
    for (auto &err : program->errors)
      tout << das::reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
    std::cout.flush();
    return false;
  }

  ctx = eastl::move(tmpCtx);

  return true;
}

static bool reload_scripts()
{
  if (!isDasInitScriptLoaded)
  {
    auto fAccess = das::make_smart<das::FsFileAccess>(dasProject, das::make_smart<das::FsFileAccess>());
    DasContextPtr ctx;
    das::ModuleGroup libGroup;
    isDasInitScriptLoaded = compile_and_simulate_script(ctx, dasInitScript, libGroup, fAccess);
  }

  if (!isDasInitScriptLoaded)
    return false;

  struct stat st;
  if (::stat(dasMainScript, &st) == 0)
  {
    if (st.st_mtime == dasMainLastModified)
      return true;
    dasMainLastModified = st.st_mtime;
  }
  else
  {
    ASSERT(false);
    return false;
  }

  auto fAccess = das::make_smart<das::FsFileAccess>(dasProject, das::make_smart<das::FsFileAccess>());
  das::ModuleGroup libGroup;
  libGroup.setUserData(new EcsModuleGroupData);
  const bool res = compile_and_simulate_script(dasCtx, dasMainScript, libGroup, fAccess);
  if (res)
  {
    for (auto kv : dasMainSystems)
      g_mgr->deleteSystem(kv.second);

    for (QueryId qid : dasMainQueries)
      g_mgr->deleteQuery(qid);

    dasMainSystems.clear();
    dasMainQueries.clear();

    auto *moduleData = ((EcsModuleGroupData*)libGroup.getUserData("ecs"));
    dasMainQueries = moduleData->dasQueries;

    for (auto &s : moduleData->unresolvedSystems)
    {
      auto res = dasMainSystems.insert(s.first);

      SystemId sid = g_mgr->createSystem(s.first.c_str(), s.second.systemDesc.release(), &s.second.queryDesc);
      res.first->second = sid;

      g_mgr->queries[g_mgr->systems[sid.index].queryId.index].userData = eastl::move(s.second.queryData);
    }

    for (auto kv : dasMainSystems)
    {
      SystemId sid = kv.second;
      DasQueryData *queryData = (DasQueryData*)g_mgr->queries[g_mgr->systems[sid.index].queryId.index].userData.get();
      queryData->ctx = dasCtx.get();
      queryData->fn  = dasCtx->findFunction(kv.first.c_str());
    }
  }
  return res;
}

bool init_sample()
{
  extern int das_def_tab_size;
  das_def_tab_size = 2;

  // TODO: Pass from command line
  das::setDasRoot(dasRoot);

  NEED_MODULE(Module_BuiltIn);
  NEED_MODULE(Module_Strings);
  NEED_MODULE(Module_Math);
  NEED_MODULE(Module_Rtti);
  NEED_MODULE(Module_Ast);
  NEED_MODULE(Module_FIO);
  NEED_MODULE(RaylibModule);
  NEED_MODULE(ECSModule);
  NEED_MODULE(AnimModule);
  NEED_MODULE(RenderModule);
  NEED_MODULE(PhysModule);
  NEED_MODULE(SampleModule);

  return reload_scripts();
}

struct RecordDescription
{
  using offset_t = uint16_t;
  using hash_t   = uint32_t;
  using type_t   = uint32_t;
  using index_t  = int16_t;
  using ctor_t   = void(*)(uint8_t*);
  using dtor_t   = void(*)(uint8_t*);

  struct Field
  {
    hash_t hash;
    offset_t size;
    offset_t id;
  };

  // TODO: Use ska::flash_hash_map<> instead of eastl::hash_map
  using OffsetMap  = eastl::hash_map<hash_t, eastl::pair<offset_t, index_t>>;
  using FieldsList = eastl::vector<Field>;
  using NamesList  = eastl::vector<eastl::string>;
  using TypesList  = eastl::vector<type_t>;
  using CtorsList  = eastl::vector<ctor_t>;
  using DtorsList  = eastl::vector<dtor_t>;

  // This is for debug only
  NamesList names;

  FieldsList fields;

  OffsetMap offsets;
  TypesList types;
  CtorsList ctors;
  DtorsList dtors;

  offset_t totalSize = 0;

  static void noop(uint8_t*) {}

  void addField(const char *name, const offset_t size, const type_t type, const ctor_t ctor = &noop, const dtor_t dtor = &noop)
  {
    const hash_t h = hash::str(name);
    ASSERT(offsets.find(h) == offsets.end());

    offsets.insert(h).first->second = eastl::make_pair(offset_t(0), index_t(0));

    fields.push_back({h, size, (offset_t)fields.size()});
    names.push_back(name);
    types.push_back(type);
    ctors.push_back(ctor);
    dtors.push_back(dtor);
  }

  void build()
  {
    eastl::sort(fields.begin(), fields.end(),
      [](const Field &a, const Field &b)
      {
        return a.size == b.size ? (a.id < b.id) : (a.size > b.size);
      });
    
    for (const Field &f : fields)
    {
      offsets[f.hash] = eastl::make_pair(totalSize, f.id);
      totalSize += f.size;
    }
  }

  void clear()
  {
    offsets.clear();
    fields.clear();
    types.clear();
    ctors.clear();
    dtors.clear();
    totalSize = 0;
  }

  template <typename T>
  inline T& getRW(char *buffer, const ConstHashedString &name)
  {
    const auto findRes = offsets.find(name.hash);
    ASSERT(findRes != offsets.end() && ComponentType<T>::type == types[findRes->second.second]);
    return *(T*)(buffer + findRes->second.first);
  }

  template <typename T>
  inline void set(char *buffer, const ConstHashedString &name, T&& value)
  {
    getRW<T>(buffer, name) = value;
  }

  template <typename T>
  inline const T& get(char *buffer, const ConstHashedString &name)
  {
    return (const T&)getRW<T>(buffer, name);
  }

  inline void construct(char *buffer)
  {
    // TODO: Fill with zeroes. 0xBA for debug only
    ::memset(buffer, 0xBA, totalSize);

    for (const auto &kv : offsets)
    {
      const RecordDescription::offset_t offset = kv.second.first;
      const RecordDescription::index_t index   = kv.second.second;
      ctors[index]((uint8_t*)(buffer + offset));
    }
  }

  inline void desctruct(char *buffer)
  {
    for (const auto &kv : offsets)
    {
      const RecordDescription::offset_t offset = kv.second.first;
      const RecordDescription::index_t index   = kv.second.second;
      dtors[index]((uint8_t*)(buffer + offset));
    }

    // TODO: 0xBA for debug only. Do nothing
    ::memset(buffer, 0xBA, totalSize);
  }
};


struct RecordView
{
  RecordDescription &desc;
  char *ptr;

  RecordView(RecordDescription &_desc, char *_ptr) : desc(_desc), ptr(_ptr) {}

  template <typename T>
  inline T& getRW(const ConstHashedString &name)
  {
    return desc.getRW<T>(ptr, name);
  }

  template <typename T>
  inline void set(const ConstHashedString &name, T&& value)
  {
    desc.set<T>(ptr, name, eastl::forward<T>(value));
  }

  template <typename T>
  inline const T& get(const ConstHashedString &name) const
  {
    return desc.get<T>(ptr, name);
  }
};

template<typename T>
struct MallocDeleter
{
  void operator()(T* p) const
  {
    ::_aligned_free(p);
  }
};

static inline char *allocate(size_t bytes, size_t alignment = 16)
{
  return (char*)::_aligned_malloc(bytes, alignment);
}

static inline char *reallocate(char* ptr, size_t bytes, size_t alignment = 16)
{
  return (char*)::_aligned_realloc(ptr, bytes, alignment);
}

struct Record
{
  RecordDescription desc;

  eastl::unique_ptr<char, MallocDeleter<char>> buffer;

  Record() = default;

  Record(const RecordDescription &_desc)
  {
    build(_desc);
  }

  Record(const Record &rhs) : desc(rhs.desc)
  {
    if (rhs.buffer)
    {
      buffer.reset(allocate(desc.totalSize));
      ::memcpy(buffer.get(), rhs.buffer.get(), desc.totalSize);
    }
  }

  ~Record()
  {
    clear();
  }

  void build(const RecordDescription &_desc)
  {
    ASSERT(!buffer);

    desc = _desc;

    buffer.reset(allocate(desc.totalSize));
    desc.construct(buffer.get());
  }

  void clear()
  {
    if (buffer)
    {
      desc.desctruct(buffer.get());
      buffer.reset();
    }

    desc.clear();
  }

  inline RecordView getView()
  {
    return RecordView(desc, buffer.get());
  }
};

struct RecordsList
{
  struct Iterator
  {
    RecordDescription &desc;
    char *ptr;

    Iterator(RecordDescription &_desc, char *_ptr) : desc(_desc), ptr(_ptr) {}

    inline const bool operator==(const Iterator &rhs) const
    {
      return ptr == rhs.ptr;
    }

    inline const bool operator!=(const Iterator &rhs) const
    {
      return ptr != rhs.ptr;
    }

    inline Iterator operator+(const int32_t &add) const
    {
      return Iterator(desc, ptr + add * desc.totalSize);
    }

    inline int32_t operator-(const Iterator &rhs) const
    {
      return int32_t((intptr_t(ptr) - intptr_t(rhs.ptr)) / intptr_t(desc.totalSize));
    }

    inline void operator++()
    {
      ptr += desc.totalSize;
    }

    inline RecordView operator*()
    {
      return RecordView(desc, ptr);
    }
  };

  RecordDescription desc;

  size_t length = 0;
  eastl::unique_ptr<char, MallocDeleter<char>> buffer;

  RecordsList() = default;

  RecordsList(const RecordDescription &_desc)
  {
    build(_desc);
  }

  RecordsList(const RecordsList &rhs) : desc(rhs.desc), length(rhs.length)
  {
    if (rhs.buffer)
    {
      buffer.reset(allocate(desc.totalSize * length));
      ::memcpy(buffer.get(), rhs.buffer.get(), desc.totalSize * length);
    }
  }

  ~RecordsList()
  {
    clear();
  }

  void build(const RecordDescription &_desc)
  {
    ASSERT(!buffer);
    desc = _desc;
  }

  void clear()
  {
    if (buffer)
    {
      for (auto item : *this)
        desc.desctruct(item.ptr);
      buffer.reset();
    }

    length = 0;
    desc.clear();
  }

  size_t append(size_t count = 1)
  {
    if (buffer)
    {
      const size_t newLength = length + count;
      char *oldMem = buffer.release();
      buffer.reset(reallocate(oldMem, desc.totalSize * newLength));

      if (buffer.get() != oldMem)
      {
        // DEBUG
        ::memset(oldMem, 0xBA, desc.totalSize * length);
      }

      for (Iterator first = begin() + length, last = first + count; first != last; ++first)
        desc.construct(first.ptr);

      const size_t oldLength = length;
      length = newLength;
      return oldLength;
    }

    length = count;
    buffer.reset(allocate(desc.totalSize * length));

    for (auto item : *this)
      desc.construct(item.ptr);

    return 0;
  }

  // Need this? Do not preserve order
  void eraseFast(size_t at, size_t count = 1)
  {
    if (length == 0)
      return;

    char *tmp = (char*)::_alloca(desc.totalSize);
    char *toErase = back().ptr;
    for (Iterator first = begin() + at, last = (at + count <= length ? first + count : end()); first != last; ++first, --length)
    {
      ::memcpy(tmp, toErase, desc.totalSize);
      ::memmove(toErase, first.ptr, desc.totalSize);
      desc.desctruct(toErase);
      ::memcpy(first.ptr, tmp, desc.totalSize);
    }
  }

  void erase(size_t at, size_t count = 1)
  {
    if (length == 0)
      return;

    const Iterator curEnd = end();
    const Iterator first  = begin() + at;
    const Iterator last   = (at + count <= length ? first + count : curEnd);
    for (Iterator i = first; i != last; ++i, --length)
      desc.desctruct(i.ptr);

    if (last != curEnd)
      ::memmove(first.ptr, last.ptr, desc.totalSize * (curEnd - last));
  }

  void insert(size_t at, size_t count = 1)
  {
    // TODO: ...
  }

  inline RecordView operator[](size_t i)
  {
    return RecordView(desc, buffer.get() + i * desc.totalSize);
  }

  inline Iterator begin()
  {
    return Iterator(desc, buffer.get());
  }

  inline Iterator end()
  {
    return begin() + length;
  }

  inline RecordView back()
  {
    return RecordView(desc, buffer.get() + (length - 1) * desc.totalSize);
  }
};

template<typename T>
struct Init
{
  static void ctor(uint8_t *mem)
  {
    new (mem) T();
  }

  static void dtor(uint8_t *mem)
  {
    ((T*)mem)->~T();
  }
};

void test_struct()
{
  RecordDescription desc;
  desc.addField("i", sizeof(int), ComponentType<int>::type);
  desc.addField("b0", sizeof(bool), ComponentType<bool>::type);
  desc.addField("f", sizeof(float), ComponentType<float>::type);
  desc.addField("b1", sizeof(bool), ComponentType<bool>::type);
  desc.addField("s", sizeof(eastl::string), ComponentType<eastl::string>::type, &Init<eastl::string>::ctor, &Init<eastl::string>::dtor);
  desc.build();

  Record rec(desc);
  RecordView view = rec.getView();
  view.set(HASH("b0"), true);
  view.set(HASH("b1"), false);
  view.set(HASH("f"), 3.141592f);
  view.set(HASH("i"), 10);
  view.set(HASH("s"), eastl::string("mysso"));

  Record copy = rec;
  rec.clear();

  RecordView copyView    = copy.getView();
  const bool b0          = copyView.get<bool>(HASH("b0"));
  const bool b1          = copyView.get<bool>(HASH("b1"));
  const float f          = copyView.get<float>(HASH("f"));
  const int i            = copyView.get<int>(HASH("i"));
  const eastl::string &s = copyView.get<eastl::string>(HASH("s"));

  const float *ptr = &copyView.getRW<float>(HASH("f"));

  RecordsList recs(desc);
  recs.append();
  recs[0].set(HASH("s"), eastl::string("RecordsList[0]"));
  recs.append();
  recs[1].set(HASH("s"), eastl::string("RecordsList[1]"));
  recs.append();
  recs[2].set(HASH("s"), eastl::string("RecordsList[2]"));
  recs.append(2);
  recs[3].set(HASH("s"), eastl::string("RecordsList[3]"));
  recs[4].set(HASH("s"), eastl::string("RecordsList[4]"));

  recs.erase(1, 2);

  const size_t at = recs.append(2);
  recs[at + 0].set(HASH("s"), eastl::string("RecordsList[5]"));
  recs[at + 1].set(HASH("s"), eastl::string("RecordsList[6]"));

  for (auto item : recs)
  {
    const eastl::string &itemStr = item.get<eastl::string>(HASH("s"));
    DEBUG_LOG(itemStr.c_str());
  }

  return;
}

int main(int argc, char *argv[])
{
  stacktrace::init();

  std::srand(unsigned(std::time(0)));

  // TODO: Update queries after templates registratina has been done
  ecs::init();

  // test_struct();

  init_sample();

  // SetConfigFlags(FLAG_WINDOW_UNDECORATED);
  InitWindow(screen_width, screen_height, "ECS Sample");

  SetTargetFPS(60);

  const float hw = 0.5f * screen_width;
  const float hh = 0.5f * screen_height;
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = /* 0.15f */ 1.0f;

  double nextResetMinMax = 0.0;

  float minDelta = 1000.f;
  float maxDelta = 0.f;

  float minRenderDelta = 1000.f;

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickMouseLeftButton{glm::vec2(pos.x, pos.y)});
    }
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickMouseLeftButton{glm::vec2(pos.x, pos.y)});
    }
    if (IsKeyPressed(KEY_SPACE))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickSpace{glm::vec2(pos.x, pos.y)});
    }
    if (IsKeyPressed(KEY_LEFT_CONTROL))
    {
      Vector2 pos = GetMousePosition();
      ecs::send_event_broadcast(EventOnClickLeftControl{glm::vec2(pos.x, pos.y)});
    }

    if (IsKeyPressed(KEY_Q))
      ecs::send_event_broadcast(EventOnChangeCohesion{50.0f});
    if (IsKeyPressed(KEY_A))
      ecs::send_event_broadcast(EventOnChangeCohesion{-50.0f});
    if (IsKeyPressed(KEY_W))
      ecs::send_event_broadcast(EventOnChangeAlignment{50.0f});
    if (IsKeyPressed(KEY_S))
      ecs::send_event_broadcast(EventOnChangeAlignment{-50.0f});
    if (IsKeyPressed(KEY_E))
      ecs::send_event_broadcast(EventOnChangeSeparation{50.0f});
    if (IsKeyPressed(KEY_D))
      ecs::send_event_broadcast(EventOnChangeSeparation{-50.0f});
    if (IsKeyPressed(KEY_R))
      ecs::send_event_broadcast(EventOnChangeWander{50.0f});
    if (IsKeyPressed(KEY_F))
      ecs::send_event_broadcast(EventOnChangeWander{-50.0f});

    if (totalTime > nextResetMinMax)
    {
      nextResetMinMax = totalTime + 1.0;

      minDelta = 1000.f;
      maxDelta = 0.f;

      minRenderDelta = 1000.f;
    }

    if (dasCtx)
    {
      dasCtx->restart();
      dasCtx->restartHeaps(); 
    }

    reload_scripts();

    double t = GetTime();
    ecs::tick();
    // Clear FrameMem here because it might be used for Jobs
    // This is valid until all jobs live one frame
    clear_frame_mem();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    ecs::invoke_event_broadcast(EventUpdate{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    minDelta = eastl::min(minDelta, delta);
    maxDelta = eastl::max(maxDelta, delta);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode2D(camera);
    t = GetTime();
    ecs::invoke_event_broadcast(EventRender{});
    #ifdef _DEBUG
    ecs::invoke_event_broadcast(EventRenderDebug{});
    #endif
    const float renderDelta = (float)((GetTime() - t) * 1e3);
    minRenderDelta = eastl::min(minRenderDelta, renderDelta);
    EndMode2D();

    DrawText(FormatText("ECS time: %2.2f ms (%2.2f ms)", /* delta */minDelta, /* renderDelta */ minRenderDelta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawText(FormatText("FM: %d B Max: %d MB (%d kB)",
      get_frame_mem_allocated_size(),
      get_frame_mem_allocated_max_size() >> 20,
      get_frame_mem_allocated_max_size() >> 10),
      10, 70, 20, LIME);
    DrawFPS(10, 10);

    ecs::invoke_event_broadcast(EventRenderHUD{});

    // float w = screen_width;
    // float h = screen_height;
    // /* exitWindow = */ GuiWindowBox({ 0.f, 0.f, w*0.5f, h*0.5f }, "PORTABLE WINDOW");

    EndDrawing();
  }

  ecs::release();
  das::Module::Shutdown();

  extern void clear_textures();
  clear_textures();

  CloseWindow();

  stacktrace::release();

  return 0;
}