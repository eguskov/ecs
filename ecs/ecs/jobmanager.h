#pragma once

#include <EASTL/functional.h>
#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

namespace jobmanager
{
  struct Stat
  {
    struct
    {
      double total = 0.0;
      double doneTasks = 0.0;
      double doneTasksTotal = 0.0;
      double finalizeTasks = 0.0;
      double finalizeJobsToRemove = 0.0;
      double finalizeDeleteJobs = 0.0;
      double finalizeDeleteJobsTotal = 0.0;
      double popQueue = 0.0;
      double popQueueTotal = 0.0;
      double assignTasks = 0.0;
      double startWorkers = 0.0;
    } scheduler;

    struct
    {
      double createJob = 0.0;
      double startJobs = 0.0;
      double startJobQueues = 0.0;
      double doneJobsMutex = 0.0;
      double deleteJob = 0.0;
      double waitAllJobs = 0.0;
    } jm;

    struct
    {
      eastl::array<double, 16> total = {};
      eastl::array<double, 16> task = {};
      eastl::array<double, 16> done = {};
    } workers;
  };

  using callback_t = eastl::function<void(int /* from */, int /* cout */)>;

  struct JobId
  {
    static constexpr uint32_t INDEX_BITS = 24;
    static constexpr uint32_t INDEX_LIMIT = (1 << INDEX_BITS);
    static constexpr uint32_t INDEX_MASK = (1 << INDEX_BITS) - 1;

    static constexpr uint32_t GENERATION_BITS = 8;
    static constexpr uint32_t GENERATION_LIMIT = (1 << GENERATION_BITS);
    static constexpr uint32_t GENERATION_MASK = (1 << GENERATION_BITS) - 1;

    union
    {
      struct
      {
        uint32_t index : 24;
        uint32_t generation : 8;
      };
      uint32_t handle = 0;
    };

    JobId(uint32_t h = 0) : handle(h) {}
    bool operator==(const JobId &rhs) const { return handle == rhs.handle; }
    bool operator!=(const JobId &rhs) const { return handle != rhs.handle; }
    operator bool() const { return handle != 0; }
  };

  using DependencyList = eastl::fixed_vector<JobId, 16, true>;

  void init();
  void release();

  JobId add_job(int items_count, int chunk_size, const callback_t &task);
  JobId add_job(const DependencyList &dependencies, int items_count, int chunk_size, const callback_t &task);
  JobId add_job(DependencyList &&dependencies, int items_count, int chunk_size, const callback_t &task);
  // TODO: add_job(..., callback_t &&task)

  JobId add_job(const DependencyList &dependencies);
  JobId add_job(DependencyList &&dependencies);

  void wait(const JobId &jid);
  void start_jobs();
  void wait_all_jobs();

  void reset_stat();
  const Stat& get_stat();
};