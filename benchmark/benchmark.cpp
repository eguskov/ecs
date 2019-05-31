#include <ecs/ecs.h>
#include <ecs/jobmanager.h>
#include <ecs/perf.h>
#include <stages/update.stage.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <benchmark/benchmark.h>

PULL_ESC_CORE;

struct update_position
{
  ECS_RUN(const UpdateStage &stage, const glm::vec3 &vel, glm::vec3 &pos)
  {
    pos += vel * stage.dt;
  }
};

static constexpr ConstComponentDescription update_position_components[] = {
  {HASH("vel"), ComponentType<glm::vec3>::Size, ComponentDescriptionFlags::kNone},
  {HASH("pos"), ComponentType<glm::vec3>::Size, ComponentDescriptionFlags::kWrite},
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
  for (auto c = query.beginChunk(), ce = query.endChunk(); c != ce; ++c)
    for (int32_t i = c.begin(), e = c.end(); i != e; ++i)
      update_position::run(*(UpdateStage*)stage_or_event.mem,
        c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, vel), i),
        c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, pos), i));
}
static SystemDescription _reg_sys_update_position(HASH("update_position"), &update_position_run, HASH("UpdateStage"), update_position_query_desc);

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

  const JValue emptyValue;
  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", emptyValue));
    ecs::tick();
    clear_frame_mem();
  }

  UpdateStage stage = { 1.f / 60.f, 10.f };

  Query query = ecs::perform_query(update_position_query_desc);

  while (state.KeepRunning())
  {
    for (auto c = query.beginChunk(), ce = query.endChunk(); c != ce; ++c)
      for (int32_t i = c.begin(), e = c.end(); i != e; ++i)
        update_position::run(stage,
          c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, vel), i),
          c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, pos), i));
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

  const JValue emptyValue;
  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", emptyValue));
    ecs::tick();

    // HACK!!!
    // FIXME: create_entity_sync does NOT perform queries!!!
    for (auto &q : g_mgr->queries)
      ecs::perform_query(q);

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

  const JValue emptyValue;
  for (int leftCount = count; leftCount > 0; leftCount -= 4096)
  {
    for (int i = 0; i < eastl::min(leftCount, 4096); ++i)
      eids.push_back(ecs::create_entity_sync("test", emptyValue));
    ecs::tick();
    clear_frame_mem();
  }

  UpdateStage stage = { 1.f / 60.f, 10.f };

  Query query = ecs::perform_query(update_position_query_desc);

  jobmanager::callback_t task = [&query, stage](int from, int count)
  {
    for (auto c = query.beginChunk(from), ce = query.endChunk(); c != ce && count; ++c)
      for (int32_t i = c.begin(), e = c.end(); i != e && count; ++i, --count)
        update_position::run(stage,
          c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, vel), i),
          c.get<glm::vec3>(INDEX_OF_COMPONENT(update_position, pos), i));
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

int main(int argc, char** argv)
{
  ecs::init();

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();

  ecs::release();
}