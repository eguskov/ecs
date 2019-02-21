#include <ecs/ecs.h>
#include <ecs/perf.h>

#include "update.ecs.h"

PULL_ESC_CORE;

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
    PERF_TIME(Native);
    for (int i = 0; i < 1000; ++i)
    {
      for (auto &v : test)
        v.pos += v.vel * dt;
    }
  }

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    PERF_TIME(ECS);
    for (int i = 0; i < 1000; ++i)
    {
      // g_mgr->tick(UpdateStage{ dt, 10.f });
      for (auto q = query.begin(), e = query.end(); q != e; ++q)
        update_position::run(*(UpdateStage*)&stage,
          GET_COMPONENT(update_position, q, glm::vec3, vel),
          GET_COMPONENT(update_position, q, glm::vec3, pos));
    }
  }

  {
    auto stage = UpdateStage{ dt, 10.f };

    Query query;
    query.desc = update_position_query_desc;
    g_mgr->performQuery(query);

    PERF_TIME(ECS_FOREACH);
    for (int i = 0; i < 1000; ++i)
    {
      for (auto q : query)
        update_position::run(*(UpdateStage*)&stage,
          GET_COMPONENT(update_position, q, glm::vec3, vel),
          GET_COMPONENT(update_position, q, glm::vec3, pos));
    }
  }

  std::cin.get();

  return 0;
}