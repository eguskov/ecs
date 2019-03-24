#include <ecs/ecs.h>
#include <ecs/jobmanager.h>
#include <ecs/perf.h>

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

int main()
{
  using namespace std::chrono_literals;

  EntityManager::create();

  // TODO: 1. Entites
  // TODO: 2. Native loop
  // TODO: 3. Script loop
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

  {
    PERF_TIME(Create);
    for (int i = 0; i < count; ++i)
      g_mgr->createEntity("test", JValue());
    g_mgr->tick();
  }

  const float dt = 1.f / 60.f;

  {
    // PERF_TIME(JobManager64);

    const double from = 0.;
    const double to = 100.;
    const double step = 0.01;
    int freqCount = (int)::ceil((to - from) / step);
    int *freq = new int[freqCount];
    ::memset(freq, 0, sizeof(int) * freqCount);

    double minTime = -1.0;
    double maxTime = -1.0;
    time_point start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i)
    {
      time_point s = std::chrono::high_resolution_clock::now();
      jobmanager::add_job(64, count,
        [&](int from, int count)
        {
          // DEBUG_LOG("From: " << from << "; Count: " << count);
          auto b = testJobManager.begin() + from;
          auto e = testJobManager.begin() + from + count;
          for (; b != e; ++b)
            (*b).pos += (*b).vel * dt;
        });
      jobmanager::do_and_wait_all_tasks_done();

      std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - s;
      const double d = diff.count();
      if (d < minTime || minTime < 0.0)
        minTime = d;
      if (d > maxTime || maxTime < 0.0)
        maxTime = d;

      int index = (int)::floor(d / step);
      ++freq[index];
    }

    std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - start;
    const double d = diff.count();

    int maxFreq = -1;
    for (int i = 0; i < freqCount; ++i)
      if (freq[i] > maxFreq)
        maxFreq = freq[i];

    std::cout << "[" << "JobManager64" << "]: " << d << " ms " << std::endl;
    std::cout << "[" << "JobManager64" << "]: min: " << minTime << " ms " << std::endl;
    std::cout << "[" << "JobManager64" << "]: max: " << maxTime << " ms " << std::endl;
    std::cout << "[" << "JobManager64" << "]: agv: " << (d / 1000.f) << " ms " << std::endl;
    std::cout << "[" << "JobManager64" << "]: mean: " << (double(maxFreq) * step) << " ms " << std::endl;
  }

  //{
  //  // PERF_TIME(Native);
  //  double minTime = -1.0;
  //  double maxTime = -1.0;
  //  time_point start = std::chrono::high_resolution_clock::now();

  //  for (int i = 0; i < 1000; ++i)
  //  {
  //    time_point s = std::chrono::high_resolution_clock::now();

  //    for (auto &v : test)
  //      v.pos += v.vel * dt;

  //    std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - s;
  //    const double d = diff.count();
  //    if (d < minTime || minTime < 0.0)
  //      minTime = d;
  //    if (d > maxTime || maxTime < 0.0)
  //      maxTime = d;
  //  }

  //  std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - start;
  //  const double d = diff.count();
  //  std::cout << "[" << "Native" << "]: " << d << " ms " << std::endl;
  //  std::cout << "[" << "Native" << "]: min: " << minTime << " ms " << std::endl;
  //  std::cout << "[" << "Native" << "]: max: " << maxTime << " ms " << std::endl;
  //  std::cout << "[" << "Native" << "]: agv: " << (d / 1000.f) << " ms " << std::endl;
  //}

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    // PERF_TIME(ECS);
    double minTime = -1.0;
    double maxTime = -1.0;
    time_point start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i)
    {
      time_point s = std::chrono::high_resolution_clock::now();
      // g_mgr->tick(UpdateStage{ dt, 10.f });
      for (auto q = query.begin(), e = query.end(); q != e; ++q)
        update_position::run(*(UpdateStage*)&stage,
          GET_COMPONENT(update_position, q, glm::vec3, vel),
          GET_COMPONENT(update_position, q, glm::vec3, pos));

      std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - s;
      const double d = diff.count();
      if (d < minTime || minTime < 0.0)
        minTime = d;
      if (d > maxTime || maxTime < 0.0)
        maxTime = d;
    }

    std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - start;
    const double d = diff.count();
    std::cout << "[" << "ECS" << "]: " << d << " ms " << std::endl;
    std::cout << "[" << "ECS" << "]: min: " << minTime << " ms " << std::endl;
    std::cout << "[" << "ECS" << "]: max: " << maxTime << " ms " << std::endl;
    std::cout << "[" << "ECS" << "]: agv: " << (d / 1000.f) << " ms " << std::endl;
  }

  //{
  //  auto stage = UpdateStage{ dt, 10.f };

  //  Query query;
  //  query.desc = update_position_query_desc;
  //  g_mgr->performQuery(query);

  //  PERF_TIME(ECS_FOREACH);
  //  for (int i = 0; i < 1000; ++i)
  //  {
  //    for (auto q : query)
  //      update_position::run(*(UpdateStage*)&stage,
  //        GET_COMPONENT(update_position, q, glm::vec3, vel),
  //        GET_COMPONENT(update_position, q, glm::vec3, pos));
  //  }
  //}

  std::cin.get();

  return 0;
}