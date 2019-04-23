#pragma once

#include <EASTL/functional.h>
#include <EASTL/array.h>

namespace jobmanager
{
  struct Stat
  {
    struct
    {
      double futureJobsMutex = 0.0;
      double doneTasksMutex = 0.0;
      double receiveDoneTasks = 0.0;
      eastl::array<double, 6> steps = {};
    } scheduler;

    struct
    {
      double createJob = 0.0;
      double startJobs = 0.0;
      double doneJobsMutex = 0.0;
      double deleteJob = 0.0;
      double doAndWaitAllTasksDone = 0.0;
    } jm;

    struct
    {
      eastl::array<double, 16> total = {};
      eastl::array<double, 16> task = {};
      eastl::array<double, 16> currentTasksMutex = {};
      eastl::array<double, 16> currentTasksMutexTotal = {};
    } workers;
  };

  using task_t = eastl::function<void(int /* from */, int /* cout */)>;

  struct JobId
  {
    static constexpr uint32_t INDEX_BITS = 24;
    static constexpr uint32_t INDEX_LIMIT = (1 << INDEX_BITS);
    static constexpr uint32_t INDEX_MASK = (1 << INDEX_BITS) - 1;

    static constexpr uint32_t GENERATION_BITS = 8;
    static constexpr uint32_t GENERATION_MASK = (1 << GENERATION_BITS) - 1;

    union
    {
      struct
      {
        uint32_t index : 24;
        uint32_t generation : 8;
      };
      uint32_t handle = 0xFFFFFFFF;
    };

    JobId(uint32_t h = 0xFFFFFFFF) : handle(h) {}
    bool operator==(const JobId &rhs) const { return handle == rhs.handle; }
    bool operator!=(const JobId &rhs) const { return handle != rhs.handle; }
    operator bool() const { return handle != 0xFFFFFFFF; }
  };

  void init();
  void release();

  // TODO: REMOVE THIS
  JobId add_job(int items_count, const task_t &task);

  JobId add_job(int items_count, int chunk_size, const task_t &task);
  void wait(const JobId &jid);
  void start_jobs();
  void do_and_wait_all_tasks_done();

  void reset_stat();
  const Stat& get_stat();
};