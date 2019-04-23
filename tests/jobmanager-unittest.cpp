#ifdef _DEBUG
#include <gtest/gtest.h>

#include <ecs/ecs.h>
#include <ecs/jobmanager.h>

TEST(JobManager, doAndWaitAllTasksDone)
{
  static const int count = 10000;

  eastl::vector<int> data;
  data.resize(count);

  int *dataBegin = data.data();
  auto task = [dataBegin](int from, int count)
  {
    auto b = dataBegin + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) = i;
  };

  jobmanager::add_job(count, 1024, task);
  jobmanager::do_and_wait_all_tasks_done();

  for (int i = 0; i < count; ++i)
    EXPECT_EQ(i, data[i]);
}

TEST(JobManager, Dependencies)
{
  static const int count = 100;

  eastl::vector<int> data;
  data.resize(count);

  int *dataBegin = data.data();
  auto taskA = [dataBegin](int from, int count)
  {
    auto b = dataBegin + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) = i;
  };

  auto taskB = [dataBegin](int from, int count)
  {
    auto b = dataBegin + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) *= 2;
  };

  auto jobA = jobmanager::add_job(count, 1024, taskA);
  auto jobB = jobmanager::add_job({ jobA }, count, 1024, taskB);
  jobmanager::do_and_wait_all_tasks_done();

  for (int i = 0; i < count; ++i)
    EXPECT_EQ(i * 2, data[i]);
}
#endif