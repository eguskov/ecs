#include <ecs/ecs.h>
#include <ecs/jobmanager.h>
#include <ecs/perf.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "benchmark-update.h"

PULL_ESC_CORE;

using time_point = std::chrono::high_resolution_clock::time_point;

struct Test
{
  glm::vec3 pos = { 0.f, 0.f, 0.f };
  glm::vec3 vel = { 1.f, 1.f, 1.f };
};

struct TestSoA
{
  eastl::vector<glm::vec3> pos;
  eastl::vector<glm::vec3> vel;
};

struct update_position
{
  ECS_RUN(const UpdateStage &stage, const glm::vec3 &vel, glm::vec3 &pos)
  {
    pos += vel * stage.dt;
  }
};

static constexpr ConstCompDesc update_position_components[] = {
  {hash::cstr("vel"), Desc<glm::vec3>::Size},
  {hash::cstr("pos"), Desc<glm::vec3>::Size},
};
static constexpr ConstQueryDesc update_position_query_desc = {
  make_const_array(update_position_components),
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
};

template <int MEASURE_COUNT>
struct PerfMeasure
{
  static constexpr double from = 0.;
  static constexpr double to = 100000.;
  static constexpr double step = 5.;
  static constexpr int freqCount = (int)(((to - from) / step) + 0.5);

  int *freq = nullptr;

  double minTime = -1.0;
  double maxTime = -1.0;

  time_point startTotal;
  time_point lastMeasure;

  eastl::string name;

  PerfMeasure(const char *_name) : name(_name)
  {
    freq = new int[freqCount];
    ::memset(freq, 0, sizeof(int) * freqCount);

    startTotal = std::chrono::high_resolution_clock::now();
  }

  ~PerfMeasure()
  {
    std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - startTotal;
    const double d = diff.count();

    int sumFreq = 0;
    int maxFreq = -1;
    int maxFreqIndex = -1;
    for (int i = 0; i < freqCount; ++i)
    {
      sumFreq += freq[i];
      if (freq[i] > maxFreq)
      {
        maxFreq = freq[i];
        maxFreqIndex = i;
      }
    }

    printf("[%s]: %6.2f ms; min: %4.2f ms; max: %5.2f ms; avg: %3d us; mean: %5.1f us (%d/%d);\n",
      name.c_str(),
      d,
      minTime * 1e-3, maxTime * 1e-3,
      int((d / float(MEASURE_COUNT)) * 1000.f),
      double(maxFreqIndex) * step,
      maxFreq, sumFreq);

    delete[](freq);
  }

  void startMeasure()
  {
    lastMeasure = std::chrono::high_resolution_clock::now();
  }

  void stopMeasure()
  {
    std::chrono::duration<double, std::micro> diff = std::chrono::high_resolution_clock::now() - lastMeasure;
    const double d = diff.count();
    if (d < minTime || minTime < 0.0)
      minTime = d;
    if (d > maxTime || maxTime < 0.0)
      maxTime = d;

    int index = (int)::floor(d / step);
    if (index >= freqCount)
      index = freqCount - 1;
    if (index < 0)
      index = 0;
    ++freq[index];
  }
};

int main()
{
  using namespace std::chrono_literals;

  EntityManager::create();

  const int count = 50000;

  eastl::vector<Test> test;
  test.resize(count);

  eastl::vector<Test> testJobManager;
  testJobManager.resize(count);

  TestSoA testSoA;
  testSoA.pos.resize(count);
  testSoA.vel.resize(count);

  TestSoA testSoARaw;
  testSoARaw.pos.resize(count);
  testSoARaw.vel.resize(count);

  for (int i = 0; i < count; ++i)
  {
    testSoA.pos[i] = { 0.f, 0.f, 0.f };
    testSoA.vel[i] = { 1.f, 1.f, 1.f };
  }

  for (int i = 0; i < count; ++i)
  {
    testSoARaw.pos[i] = { 0.f, 0.f, 0.f };
    testSoARaw.vel[i] = { 1.f, 1.f, 1.f };
  }

  const float dt = 1.f / 60.f;

  /*{
    static const int MEASURE_COUNT = 1000;

    PerfMeasure<MEASURE_COUNT> perf("Test");

    for (int i = 0; i < MEASURE_COUNT; ++i)
    {
      perf.startMeasure();
      jobmanager::add_job(count, 4096,
        [&](int from, int count)
        {
          auto b = testJobManager.begin() + from;
          auto e = b + count;
          for (; b != e; ++b)
          {
            (*b).pos += (*b).vel * dt;
          }
        });
      jobmanager::do_and_wait_all_tasks_done();
      perf.stopMeasure();
    }
  }*/

  {
    PERF_TIME(Create);
    for (int j = 0; j < 1; ++j)
    {
      for (int i = 0; i < count; ++i)
        g_mgr->createEntity("test", JValue());
      g_mgr->tick();
      clear_frame_mem();
    }
  }

  static const int MEASURE_COUNT = 10;

  {
    PerfMeasure<MEASURE_COUNT> perf("Native   ");

    for (int i = 0; i < MEASURE_COUNT; ++i)
    {
      perf.startMeasure();
      for (auto &v : test)
        v.pos += v.vel * dt;
      perf.stopMeasure();
    }
  }

  {
    auto task = [&](int from, int count)
    {
      auto b = testJobManager.begin() + from;
      auto e = b + count;
      for (; b != e; ++b)
        (*b).pos += (*b).vel * dt;
    };

    {
      PerfMeasure<MEASURE_COUNT> perf("JM Native");
      for (int i = 0; i < MEASURE_COUNT; ++i)
      {
        perf.startMeasure();
        jobmanager::add_job(count, 1024, task);
        jobmanager::do_and_wait_all_tasks_done();
        perf.stopMeasure();
      }
    }

    // std::cout << "futureJobsMutex: " << jobmanager::get_stat().scheduler.futureJobsMutex << "ms" << "\n";

    std::cout << "doneTasksMutex: " << jobmanager::get_stat().scheduler.doneTasksMutex << "ms" << "\n";
    std::cout << "receiveDoneTasks: " << jobmanager::get_stat().scheduler.receiveDoneTasks << "ms" << "\n";
    double sum = 0.0;
    for (int i = 0; i < (int)jobmanager::get_stat().scheduler.steps.size(); ++i)
    {
      sum += jobmanager::get_stat().scheduler.steps[i];
      std::cout << "step[" << i << "]: " << jobmanager::get_stat().scheduler.steps[i] << "ms" << "\n";
    }
    std::cout << "steps: " << sum << "ms" << "\n";

    std::cout << "createJob: " << jobmanager::get_stat().jm.createJob << "ms" << "\n";
    std::cout << "startJobs: " << jobmanager::get_stat().jm.startJobs << "ms" << "\n";
    std::cout << "doneJobsMutex: " << jobmanager::get_stat().jm.doneJobsMutex << "ms" << "\n";
    std::cout << "deleteJob: " << jobmanager::get_stat().jm.deleteJob << "ms" << "\n";
    std::cout << "doAndWaitAllTasksDone: " << jobmanager::get_stat().jm.doAndWaitAllTasksDone << "ms" << "\n";

    for (int i = 0; i < (int)jobmanager::get_stat().workers.total.size(); ++i)
      if (jobmanager::get_stat().workers.total[i] > 0.0)
      {
        std::cout << "workers[" << i << "]: total: " << jobmanager::get_stat().workers.total[i] << "ms" << "\n";
        std::cout << "workers[" << i << "]: task:  " << jobmanager::get_stat().workers.task[i] << "ms" << "\n";
      }
  }

  std::cin.get();

  return 0;

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    PerfMeasure<MEASURE_COUNT> perf("JM ECS   ");
    for (int i = 0; i < MEASURE_COUNT; ++i)
    {
      perf.startMeasure();
      jobmanager::add_job(query.entitiesCount, 1024,
        [&](int from, int count)
        {
          auto begin = query.begin(from);
          auto end = query.begin(from + count);
          for (auto q = begin, e = end; q != e; ++q)
            update_position::run(*(UpdateStage*)&stage,
              GET_COMPONENT(update_position, q, glm::vec3, vel),
              GET_COMPONENT(update_position, q, glm::vec3, pos));
        });
      jobmanager::do_and_wait_all_tasks_done();
      perf.stopMeasure();
    }
  }

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    PerfMeasure<MEASURE_COUNT> perf("ECS      ");
    for (int i = 0; i < MEASURE_COUNT; ++i)
    {
      perf.startMeasure();
      for (auto q = query.begin(), e = query.end(); q != e; ++q)
        update_position::run(*(UpdateStage*)&stage,
          GET_COMPONENT(update_position, q, glm::vec3, vel),
          GET_COMPONENT(update_position, q, glm::vec3, pos));
      perf.stopMeasure();
    }
  }

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    PerfMeasure<MEASURE_COUNT> perf("ECS Feach");
    for (int i = 0; i < MEASURE_COUNT; ++i)
    {
      perf.startMeasure();
      for (auto q : query)
        update_position::run(*(UpdateStage*)&stage,
          GET_COMPONENT(update_position, q, glm::vec3, vel),
          GET_COMPONENT(update_position, q, glm::vec3, pos));
      perf.stopMeasure();
    }
  }

  std::cin.get();

  return 0;
}