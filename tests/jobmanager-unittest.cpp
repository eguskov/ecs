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
  jobmanager::wait_all_jobs();

  for (int i = 0; i < count; ++i)
    EXPECT_EQ(i, data[i]);
}

TEST(JobManager, Dependencies)
{
  static const int count = 100;

  eastl::vector<int> data;
  data.resize(count);

  int *dataBeginA1 = data.data();
  auto taskA1 = [dataBeginA1](int from, int count)
  {
    auto b = dataBeginA1 + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) = i;
  };
  int *dataBeginA2 = data.data() + count / 2;
  int totalCountA2 = count / 2;
  auto taskA2 = [dataBeginA2, totalCountA2](int from, int count)
  {
    auto b = dataBeginA2 + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) = totalCountA2 - i;
  };

  int *dataBeginB = data.data();
  auto taskB = [dataBeginB](int from, int count)
  {
    auto b = dataBeginB + from;
    auto e = b + count;
    int i = from;
    for (; b != e; ++b, ++i)
      (*b) *= 2;
  };

  auto jobA1 = jobmanager::add_job(count/2, 16, taskA1);
  auto jobA2 = jobmanager::add_job(count/2, 16, taskA2);
  jobmanager::start_jobs();

  auto jobA = jobmanager::add_job({ jobA1, jobA2 });
  jobmanager::start_jobs();

  auto jobB = jobmanager::add_job({ jobA }, count, 16, taskB);

  jobmanager::wait_all_jobs();

  for (int i = 0; i < count; ++i)
    if (i < count / 2)
      EXPECT_EQ(i * 2, data[i]);
    else
      EXPECT_EQ((count - i) * 2, data[i]);
}
#endif