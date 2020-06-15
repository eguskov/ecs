#include <ecs/ecs.h>
#include <ecs/jobmanager.h>
#include <ecs/perf.h>
#include <stages/update.stage.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <benchmark/benchmark.h>

#include <daScript/daScript.h>
#include <daScript/simulate/fs_file_info.h>

PULL_ESC_CORE;

struct update_position
{
  ECS_RUN(const UpdateStage &stage, const glm::vec3 &vel, glm::vec3 &pos)
  {
    pos += vel * stage.dt;
  }
};

static constexpr ConstComponentDescription update_position_components[] = {
  {HASH("vel"), ComponentType<glm::vec3>::size, ComponentDescriptionFlags::kNone},
  {HASH("pos"), ComponentType<glm::vec3>::size, ComponentDescriptionFlags::kWrite},
};
static constexpr ConstQueryDescription update_position_query_desc = {
  make_const_array(update_position_components),
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
};

static void update_position_run(const RawArg &stage_or_event, Query &query)
{
  ecs::wait_system_dependencies(HASH("update_position"));
  for (auto q = query.begin(), e = query.end(); q != e; ++q)
    update_position::run(*(UpdateStage*)stage_or_event.mem,
      GET_COMPONENT(update_position, q, glm::vec3, vel),
      GET_COMPONENT(update_position, q, glm::vec3, pos));
}
static SystemDescription _reg_sys_update_position(HASH("update_position"), &update_position_run, HASH("UpdateStage"), update_position_query_desc, "", "");

static void BM_NativeFor(benchmark::State& state)
{
  const float dt = 1.f / 60.f;

  const int count = (int)state.range(0);

  eastl::vector<glm::vec3> pos;
  eastl::vector<glm::vec3> vel;
  pos.resize(count, glm::vec3(0.f, 0.f, 0.f));
  vel.resize(count, glm::vec3(1.f, 1.f, 1.f));

  while (state.KeepRunning())
  {
    for (int i = 0; i < count; ++i)
      pos[i] += vel[i] * dt;
  }
}
BENCHMARK(BM_NativeFor)->RangeMultiplier(2)->Range(1 << 11, 1 << 20);

static void BM_ECS_System(benchmark::State& state)
{
  const int count = (int)state.range(0);

  eastl::vector<EntityId> eids;

  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", ComponentsMap()));
    ecs::tick();
    clear_frame_mem();
  }

  UpdateStage stage = { 1.f / 60.f, 10.f };

  Query query = ecs::perform_query(update_position_query_desc);

  while (state.KeepRunning())
  {
    for (auto q = query.begin(), e = query.end(); q != e; ++q)
      update_position::run(stage,
        GET_COMPONENT(update_position, q, glm::vec3, vel),
        GET_COMPONENT(update_position, q, glm::vec3, pos));
  }

  for (auto eid : eids)
    ecs::delete_entity(eid);
  ecs::tick();
}
BENCHMARK(BM_ECS_System)->RangeMultiplier(2)->Range(1 << 11, 1 << 20);

static void BM_ECS_UpdateStage(benchmark::State& state)
{
  const int count = (int)state.range(0);

  eastl::vector<EntityId> eids;

  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", ComponentsMap()));
    ecs::tick();

    // HACK!!!
    // FIXME: create_entity_sync does NOT perform queries!!!
    for (auto &sys : g_mgr->systems)
      ecs::perform_query(sys.queryId);

    clear_frame_mem();
  }

  while (state.KeepRunning())
  {
    ecs::tick(UpdateStage{ 1.f / 60.f, 10.f });
  }

  for (auto eid : eids)
    ecs::delete_entity(eid);
  ecs::tick();
}
BENCHMARK(BM_ECS_UpdateStage)->RangeMultiplier(2)->Range(1 << 11, 1 << 20);

static void BM_ScriptForDas(benchmark::State& state)
{
  const int count = (int)state.range(0);

  auto fAccess = das::make_smart<das::FsFileAccess>();

  das::TextPrinter tout;
  das::ModuleGroup dummyLibGroup;

  auto program = das::compileDaScript("D:/projects/ecs/benchmark/benchmark.das", fAccess, tout, dummyLibGroup);

  das::Context ctx(program->getContextStackSize());
  if (!program->simulate(ctx, tout))
  {
    for (auto & err : program->errors)
      tout << das::reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
  }

  auto fnTest = ctx.findFunction("test");

  eastl::vector<glm::vec3> pos;
  eastl::vector<glm::vec3> vel;
  pos.resize(count, glm::vec3(0.f, 0.f, 0.f));
  vel.resize(count, glm::vec3(1.f, 1.f, 1.f));

  if (fnTest)
    while (state.KeepRunning())
    {
      for (int i = 0; i < count; ++i)
      {
        ctx.restart();
        vec4f args[] = { das::cast<char *>::from((char *)&pos[i]), das::cast<char *>::from((char *)&vel[i]) };
        ctx.eval(fnTest, args);
      }
    }
}
BENCHMARK(BM_ScriptForDas)->RangeMultiplier(2)->Range(1 << 11, 1 << 20);

static void BM_ScriptForDasAot(benchmark::State& state)
{
  const int count = (int)state.range(0);

  auto fAccess = das::make_smart<das::FsFileAccess>();

  das::TextPrinter tout;
  das::ModuleGroup dummyLibGroup;

  auto program = das::compileDaScript("D:/projects/ecs/benchmark/benchmark.das", fAccess, tout, dummyLibGroup);

  das::Context ctx(program->getContextStackSize());
  if (!program->simulate(ctx, tout))
  {
    for (auto & err : program->errors)
      tout << das::reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
  }

  das::AotLibrary aotLib;
  das::AotListBase::registerAot(aotLib);
  program->linkCppAot(ctx, aotLib, tout);

  auto fnTest = ctx.findFunction("test");

  eastl::vector<glm::vec3> pos;
  eastl::vector<glm::vec3> vel;
  pos.resize(count, glm::vec3(0.f, 0.f, 0.f));
  vel.resize(count, glm::vec3(1.f, 1.f, 1.f));

  if (fnTest)
    while (state.KeepRunning())
    {
      for (int i = 0; i < count; ++i)
      {
        ctx.restart();
        vec4f args[] = {
          das::cast<glm::vec3*>::from(&pos[i]),
          das::cast<const glm::vec3*>::from(&vel[i]),
        };
        ctx.eval(fnTest, args);
      }
    }
}
BENCHMARK(BM_ScriptForDasAot)->RangeMultiplier(2)->Range(1 << 11, 1 << 20);

static void BM_JobManager(benchmark::State& state)
{
  const float dt = 1.f / 60.f;

  const int count = (int)state.range(0);

  eastl::vector<glm::vec3> pos;
  eastl::vector<glm::vec3> vel;
  pos.resize(count, glm::vec3(0.f, 0.f, 0.f));
  vel.resize(count, glm::vec3(1.f, 1.f, 1.f));

  auto posData = pos.data();
  auto velData = vel.data();
  auto task = [posData, velData, dt](int from, int count)
  {
    for (int i = from, end = from + count; i < end; ++i)
      posData[i] += posData[i] * dt;
  };

  const int chunkSize = (int)state.range(1);

  jobmanager::reset_stat();

  while (state.KeepRunning())
  {
    jobmanager::add_job(count, chunkSize, task);
    jobmanager::wait_all_jobs();
  }
}
BENCHMARK(BM_JobManager)->RangeMultiplier(2)->Ranges({{1 << 11, 1 << 20}, {256, 1024}});

static void BM_ECS_JobManager(benchmark::State& state)
{
  const int count = (int)state.range(0);

  eastl::vector<EntityId> eids;

  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", ComponentsMap()));
    ecs::tick();
    clear_frame_mem();
  }

  UpdateStage stage = { 1.f / 60.f, 10.f };

  Query query = ecs::perform_query(update_position_query_desc);

  jobmanager::callback_t task = [&query, stage](int from, int count)
  {
    for (auto q = query.begin(from), e = query.end(); q != e && count > 0; ++q, --count)
      update_position::run(stage,
        GET_COMPONENT(update_position, q, glm::vec3, vel),
        GET_COMPONENT(update_position, q, glm::vec3, pos));
  };

  const int chunkSize = (int)state.range(1);

  jobmanager::reset_stat();

  while (state.KeepRunning())
  {
    jobmanager::add_job(count, chunkSize, task);
    jobmanager::wait_all_jobs();
  }

  for (auto eid : eids)
    ecs::delete_entity(eid);
  ecs::tick();
}
BENCHMARK(BM_ECS_JobManager)->RangeMultiplier(2)->Ranges({{1 << 11, 1 << 20}, {256, 1024}});

#include <Windows.h>

#include <condition_variable>
#include <mutex>

static void BM_ThreadCondVar(benchmark::State& state)
{
  struct Worker
  {
    std::atomic<bool> working{true};
    std::thread thread;

    bool start = false;
    std::condition_variable startCV;
    std::mutex startMutex;

    bool done = false;
    std::condition_variable doneCV;
    std::mutex doneMutex;
  };

  eastl::array<Worker, 8> workers;

  auto mainThreadId = std::this_thread::get_id();
  auto mainCpuNo = ::GetCurrentProcessorNumber();

  ::SetThreadAffinityMask(::GetCurrentThread(), 1 << mainCpuNo);

  const int workersCount = std::thread::hardware_concurrency() - 1;

  DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
  for (int i = 0; i < workersCount; ++i, ++cpuNo)
  {
    workers[i].thread = eastl::move(std::thread([&](int wid)
    {
      auto &w = workers[wid];
      while (w.working.load())
      {
        {
          std::unique_lock<std::mutex> lock(w.startMutex);
          w.startCV.wait(lock, [&] { return w.start; });
          w.start = false;
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

        {
          std::lock_guard<std::mutex> lock(w.doneMutex);
          w.done = true;
        }
        w.doneCV.notify_one();
      }
    }, i));

    HANDLE handle = (HANDLE)workers[i].thread.native_handle();

    if (cpuNo == mainCpuNo)
      ++cpuNo;

    ::SetThreadAffinityMask(handle, 1 << cpuNo);
  }

  for (auto _ : state)
  {
    for (int i = 0; i < workersCount; ++i)
    {
      {
        std::lock_guard<std::mutex> lock(workers[i].startMutex);
        workers[i].start = true;
      }
      workers[i].startCV.notify_one();
    }

    for (int i = 0; i < workersCount; ++i)
    {
      std::unique_lock<std::mutex> lock(workers[i].doneMutex);
      workers[i].doneCV.wait(lock, [&] { return workers[i].done; });
      workers[i].done = false;
    }
  }

  for (int i = 0; i < workersCount; ++i)
  {
    workers[i].working = false;

    {
      std::lock_guard<std::mutex> lock(workers[i].startMutex);
      workers[i].start = true;
    }
    workers[i].startCV.notify_one();

    workers[i].thread.join();
  }
}
BENCHMARK(BM_ThreadCondVar);

static void BM_ThreadBusyWait(benchmark::State& state)
{
  struct Worker
  {
    std::atomic<bool> working{true};
    std::thread thread;

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
  };

  eastl::array<Worker, 8> workers;

  auto mainThreadId = std::this_thread::get_id();
  auto mainCpuNo = ::GetCurrentProcessorNumber();

  ::SetThreadAffinityMask(::GetCurrentThread(), 1 << mainCpuNo);

  const int workersCount = std::thread::hardware_concurrency() - 1;

  DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
  for (int i = 0; i < workersCount; ++i, ++cpuNo)
  {
    workers[i].thread = eastl::move(std::thread([&](int wid)
    {
      auto &w = workers[wid];
      while (w.working.load())
      {
        while (!w.start.load())
        {
          std::this_thread::yield();
        }
        w.start = false;

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

        w.done = true;
      }
    }, i));

    HANDLE handle = (HANDLE)workers[i].thread.native_handle();

    if (cpuNo == mainCpuNo)
      ++cpuNo;

    ::SetThreadAffinityMask(handle, 1 << cpuNo);
  }

  for (auto _ : state)
  {
    for (int i = 0; i < workersCount; ++i)
      workers[i].start = true;

    for (int i = 0; i < workersCount; ++i)
    {
      while (!workers[i].done.load())
      {
        std::this_thread::yield();
      }
      workers[i].done = false;
    }
  }

  for (int i = 0; i < workersCount; ++i)
  {
    workers[i].working = false;
    workers[i].start = true;
    workers[i].thread.join();
  }
}
BENCHMARK(BM_ThreadBusyWait);

static void BM_ThreadEvent(benchmark::State& state)
{
  struct Worker
  {
    std::atomic<bool> working{true};
    std::thread thread;

    HANDLE startEvent = NULL;
    HANDLE doneEvent = NULL;
  };

  eastl::array<Worker, 8> workers;

  auto mainThreadId = std::this_thread::get_id();
  auto mainCpuNo = ::GetCurrentProcessorNumber();

  ::SetThreadAffinityMask(::GetCurrentThread(), 1 << mainCpuNo);

  const int workersCount = std::thread::hardware_concurrency() - 1;

  DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
  for (int i = 0; i < workersCount; ++i, ++cpuNo)
  {
    workers[i].startEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    workers[i].doneEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    workers[i].thread = eastl::move(std::thread([&](int wid)
    {
      auto &w = workers[wid];
      while (w.working.load())
      {
        ::WaitForSingleObjectEx(w.startEvent, INFINITE, TRUE);

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

        ::SetEvent(w.doneEvent);
      }
    }, i));

    HANDLE handle = (HANDLE)workers[i].thread.native_handle();

    if (cpuNo == mainCpuNo)
      ++cpuNo;

    ::SetThreadAffinityMask(handle, 1 << cpuNo);
  }

  for (auto _ : state)
  {
    for (int i = 0; i < workersCount; ++i)
      ::SetEvent(workers[i].startEvent);

    state.PauseTiming();
    for (int i = 0; i < workersCount; ++i)
      ::WaitForSingleObjectEx(workers[i].doneEvent, INFINITE, TRUE);
    state.ResumeTiming();
  }

  for (int i = 0; i < workersCount; ++i)
  {
    workers[i].working = false;
    ::SetEvent(workers[i].startEvent);
  
    workers[i].thread.join();

    ::CloseHandle(workers[i].startEvent);
    ::CloseHandle(workers[i].doneEvent);
  }
}
BENCHMARK(BM_ThreadEvent);

static void BM_ThreadEventWithBusyWait(benchmark::State& state)
{
  static constexpr int busyWaitCycles = 1024;

  struct Worker
  {
    std::atomic<bool> working{true};
    std::thread thread;

    HANDLE startEvent = NULL;
    HANDLE doneEvent = NULL;

    std::atomic<bool> done{false};
  };

  eastl::array<Worker, 8> workers;

  auto mainThreadId = std::this_thread::get_id();
  auto mainCpuNo = ::GetCurrentProcessorNumber();

  ::SetThreadAffinityMask(::GetCurrentThread(), 1 << mainCpuNo);

  const int workersCount = std::thread::hardware_concurrency() - 1;

  DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
  for (int i = 0; i < workersCount; ++i, ++cpuNo)
  {
    workers[i].startEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    workers[i].doneEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    workers[i].thread = eastl::move(std::thread([&](int wid)
    {
      auto &w = workers[wid];
      while (w.working.load())
      {
        ::WaitForSingleObjectEx(w.startEvent, INFINITE, TRUE);

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

        w.done = true;
        ::SetEvent(w.doneEvent);
      }
    }, i));

    HANDLE handle = (HANDLE)workers[i].thread.native_handle();

    if (cpuNo == mainCpuNo)
      ++cpuNo;

    ::SetThreadAffinityMask(handle, 1 << cpuNo);
  }

  for (auto _ : state)
  {
    for (int i = 0; i < workersCount; ++i)
      ::SetEvent(workers[i].startEvent);

    state.PauseTiming();
    for (int i = 0; i < workersCount; ++i)
    {
      int cycles = busyWaitCycles;
      while (--cycles && !workers[i].done.load())
      {
        std::this_thread::yield();
      }

      if (workers[i].done.load())
        ::WaitForSingleObjectEx(workers[i].doneEvent, 0, TRUE);
      else
        ::WaitForSingleObjectEx(workers[i].doneEvent, INFINITE, TRUE);

      workers[i].done = false;
    }
    state.ResumeTiming();
  }

  for (int i = 0; i < workersCount; ++i)
  {
    workers[i].working = false;
    ::SetEvent(workers[i].startEvent);
  
    workers[i].thread.join();

    ::CloseHandle(workers[i].startEvent);
    ::CloseHandle(workers[i].doneEvent);
  }
}
BENCHMARK(BM_ThreadEventWithBusyWait);

static void BM_ThreadEventMask(benchmark::State& state)
{
  static constexpr int busyWaitCycles = 1024;

  struct Worker
  {
    std::atomic<bool> working{true};
    std::thread thread;

    HANDLE startEvent = NULL;
  };

  eastl::array<Worker, 8> workers;

  std::mutex doneMutex;
  eastl::bitset<8> doneWorkers;
  doneWorkers.reset();

  HANDLE doneEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  auto mainThreadId = std::this_thread::get_id();
  auto mainCpuNo = ::GetCurrentProcessorNumber();

  ::SetThreadAffinityMask(::GetCurrentThread(), 1 << mainCpuNo);

  const int workersCount = std::thread::hardware_concurrency() - 1;

  DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
  for (int i = 0; i < workersCount; ++i, ++cpuNo)
  {
    workers[i].startEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    workers[i].thread = eastl::move(std::thread([&](int wid)
    {
      auto &w = workers[wid];
      while (w.working.load())
      {
        ::WaitForSingleObjectEx(w.startEvent, INFINITE, TRUE);

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

        {
          std::lock_guard<std::mutex> lock(doneMutex);
          doneWorkers.set(wid);
        }
        ::SetEvent(doneEvent);
      }
    }, i));

    HANDLE handle = (HANDLE)workers[i].thread.native_handle();

    if (cpuNo == mainCpuNo)
      ++cpuNo;

    ::SetThreadAffinityMask(handle, 1 << cpuNo);
  }

  for (auto _ : state)
  {
    for (int i = 0; i < workersCount; ++i)
      ::SetEvent(workers[i].startEvent);

    state.PauseTiming();
    bool allDone = false;
    while (!allDone)
    {
      ::WaitForSingleObjectEx(doneEvent, INFINITE, TRUE);

      allDone = true;

      {
        std::lock_guard<std::mutex> lock(doneMutex);
        for (int i = 0; i < workersCount; ++i)
          if (!doneWorkers[i])
          {
            allDone = false;
            break;
          }
      }
    }
    doneWorkers.reset();
    state.ResumeTiming();
  }

  for (int i = 0; i < workersCount; ++i)
  {
    workers[i].working = false;
    ::SetEvent(workers[i].startEvent);
  
    workers[i].thread.join();

    ::CloseHandle(workers[i].startEvent);
  }
  ::CloseHandle(doneEvent);
}
BENCHMARK(BM_ThreadEventMask);

int main(int argc, char** argv)
{
  // ecs::init();

  extern int das_def_tab_size;
  das_def_tab_size = 2;

  NEED_MODULE(Module_BuiltIn);
  NEED_MODULE(Module_Math);

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();

  // ecs::release();

  das::Module::Shutdown();
}