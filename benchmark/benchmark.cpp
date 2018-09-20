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

int main()
{
  using namespace std::chrono_literals;

  EntityManager::init();

  // TODO: 1. Entites
  // TODO: 2. Native loop
  // TODO: 3. Script loop
  const int count = 30000;

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
    for (auto &v : test)
      v.pos += v.vel * dt;
  }

  {
    PERF_TIME(Native_SoA);
    for (int i = 0; i < count; ++i)
    {
      testSoA.pos[i] += testSoA.vel[i] * dt;
    }
  }

  {
    uint8_t *posRaw = (uint8_t *)testSoARaw.pos.data();
    uint8_t *velRaw = (uint8_t *)testSoARaw.vel.data();
    PERF_TIME(Native_SoA_RAW);
    for (int i = 0; i < count; ++i)
    {
      *(glm::vec3*)(posRaw + i * sizeof(glm::vec3)) += (*(glm::vec3*)(velRaw + i * sizeof(glm::vec3))) * dt;
    }
  }

  {
    PERF_TIME(ECS);
    g_mgr->tick(UpdateStage{ dt, 10.f });
  }

  std::cin.get();

  return 0;
}