#define VER 2

#if VER == 2

#include "jobmanager-v2.cpp"

#else

#include "jobmanager.h"

#include "debug.h"
#include "framemem.h"

#include <Windows.h>

#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/algorithm.h>
#include <EASTL/bitset.h>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>

using JobId = jobmanager::JobId;

static inline JobId make_jid(uint8_t gen, uint32_t index)
{
  return JobId(((uint32_t)gen << JobId::INDEX_BITS | index));
}

static jobmanager::Stat g_stat;

template <typename Period>
struct ScopeTime
{
  double &target;

  std::chrono::high_resolution_clock::time_point start;

  ScopeTime(double &_target) : target(_target)
  {
    start = std::chrono::high_resolution_clock::now();
  }

  ~ScopeTime()
  {
    std::chrono::duration<double, Period> diff = std::chrono::high_resolution_clock::now() - start;
    target += diff.count();
  }
};

using ScopeTimeMS = ScopeTime<std::milli>;
using ScopeTimeNS = ScopeTime<std::nano>;

struct JobManager
{
  struct Job
  {
    int tasksCount = 0;

    int itemsCount = 0;
    int chunkSize = 0;

    jobmanager::task_t task;
  };

  struct Task
  {
    JobId jid;
    jobmanager::task_t task;
    int from;
    int count;
  };

  struct Worker
  {
    std::mutex futureTasksMutex;
    std::condition_variable futureTasksCV;
    eastl::vector<Task> futureTasks;

    std::mutex doneTasksMutex;
    eastl::vector<JobId> doneTasks;

    eastl::queue<Task> tasks;

    std::atomic<bool> terminated = false;
    bool started = false;
  };

  int workersCount = 0;
  int doneTasksCount = 0;
  int processedTasksCount = 0;

  std::thread::id mainThreadId;
  uint32_t mainCpuNo = 0;

  std::mutex startWorkerMutex;
  std::condition_variable startWorkerCV;

  std::mutex doneWorkerMutex;
  std::condition_variable doneWorkerCV;

  eastl::array<Worker, 16> workers;
  eastl::array<std::thread, 16> workersThread;

  struct Scheduler
  {
    std::mutex futureJobsMutex;
    eastl::vector<JobId> futureJobIds;
    eastl::vector<Job> futureJobs;
    eastl::vector<eastl::vector<JobId>> futureJobDependencies;

    eastl::vector<JobId> jobIds;
    eastl::vector<Job> jobs;
    eastl::vector<eastl::vector<JobId>> jobDependencies;

    std::mutex doneJobsMutex;
    eastl::vector<JobId> doneJobs;
    std::condition_variable doneJobsCV;

    std::mutex mutex;
    std::condition_variable cv;

    std::atomic<bool> terminated = false;
    bool started = false;
  } scheduler;

  std::thread schedulerThread;

  eastl::vector<Task> tasks;
  eastl::deque<uint32_t> freeJobQueue;

  int jobsCount = 0;
  eastl::vector<Job> jobs;
  eastl::vector<uint8_t> jobGenerations;
  eastl::vector<eastl::vector<JobId>> jobDependencies;

  eastl::vector<JobId> jobsToStart;

  static void worker_routine(JobManager *g_jm, int worker_id)
  {
    Worker &worker = g_jm->workers[worker_id];

    while (!worker.terminated.load())
    {
      {
        std::unique_lock<std::mutex> lock(worker.futureTasksMutex);
        worker.futureTasksCV.wait(lock, [&](){ return !worker.futureTasks.empty(); });

        for (int i = 0; i < (int)worker.futureTasks.size(); ++i)
          worker.tasks.push(worker.futureTasks[i]);
        worker.futureTasks.clear();
      }

      ScopeTimeMS _time(g_stat.workers.total[worker_id]);

      while (!worker.tasks.empty())
      {
        Task task = worker.tasks.front();
        worker.tasks.pop();

        {
          ScopeTimeMS _time(g_stat.workers.task[worker_id]);
          task.task(task.from, task.count);
        }

        {
          std::lock_guard<std::mutex> lock(worker.doneTasksMutex);
          worker.doneTasks.push_back(task.jid);
        }

        {
          std::lock_guard<std::mutex> lock(g_jm->doneWorkerMutex);
          ++g_jm->doneTasksCount;
        }
        g_jm->doneWorkerCV.notify_one();
      }
    }
  }

  static void scheduler_routine(JobManager *g_jm)
  {
    Scheduler &scheduler = g_jm->scheduler;
    while (!scheduler.terminated.load())
    {
      {
        ScopeTimeMS _time(g_stat.scheduler.futureJobsMutex);

        std::unique_lock<std::mutex> lock(scheduler.futureJobsMutex);
        scheduler.cv.wait(lock, [&](){ return !scheduler.futureJobs.empty(); });
      }

      // 0. Copy future jobs to current
      // 1. Find jobs without dependencies
      // 2. Create tasks for each job
      // 3. Send tasks to workers
      // 4. Receive done tasks from workers
      // 5. Send done jobs
      // 6. Goto 1.

      // 0.
      {
        ScopeTimeMS _time(g_stat.scheduler.steps[0]);

        std::lock_guard<std::mutex> lock(scheduler.futureJobsMutex);
        for (int i = 0; i < (int)scheduler.futureJobIds.size(); ++i)
        {
          scheduler.jobIds.push_back(scheduler.futureJobIds[i]);
          scheduler.jobs.push_back(scheduler.futureJobs[i]);
          scheduler.jobDependencies.push_back(scheduler.futureJobDependencies[i]);
        }
        scheduler.futureJobIds.clear();
        scheduler.futureJobs.clear();
        scheduler.futureJobDependencies.clear();
      }

      eastl::fixed_vector<int, 128, true> jobsToStart;
      // 1.
      {
        ScopeTimeMS _time(g_stat.scheduler.steps[1]);

        for (int i = 0; i < (int)scheduler.jobs.size(); ++i)
          jobsToStart.push_back(i);
      }

      // 2.
      eastl::vector<Task> tasksToStart;
      {
        ScopeTimeMS _time(g_stat.scheduler.steps[2]);

        for (int jobIdx : jobsToStart)
        {
          const JobId &jid = scheduler.jobIds[jobIdx];
          Job &job = scheduler.jobs[jobIdx];

          int itemsLeft = job.itemsCount;
          job.tasksCount = (job.itemsCount / job.chunkSize) + ((job.itemsCount % job.chunkSize) ? 1 : 0);
          tasksToStart.reserve(job.tasksCount);
          for (int i = 0; i < job.itemsCount; i += job.chunkSize, itemsLeft -= job.chunkSize)
          {
            Task &t = tasksToStart.push_back();
            t.jid = jid;
            t.task = job.task;
            t.from = i;
            t.count = eastl::min(job.chunkSize, itemsLeft);
          }
        }
      }

      // 3.
      {
        ScopeTimeMS _time(g_stat.scheduler.steps[3]);
        int workerNo = 0;
        for (const Task &task : tasksToStart)
        {
          {
            std::lock_guard<std::mutex> lock(g_jm->workers[workerNo].futureTasksMutex);
            g_jm->workers[workerNo].futureTasks.push_back(task);
          }
          g_jm->workers[workerNo].futureTasksCV.notify_one();

          workerNo = (workerNo + 1) % g_jm->workersCount;
        }
      }

      // 4.
      {
        ScopeTimeMS _time(g_stat.scheduler.steps[4]);
        while (!scheduler.jobs.empty())
        {
          {
            ScopeTimeMS _time(g_stat.scheduler.doneTasksMutex);
            std::unique_lock<std::mutex> lock(g_jm->doneWorkerMutex);
            g_jm->doneWorkerCV.wait(lock, [&] { return g_jm->doneTasksCount > 0; });
            g_jm->doneTasksCount = 0;
          }

          eastl::vector<JobId> doneTasks;
          {
            ScopeTimeMS _time(g_stat.scheduler.receiveDoneTasks);
            for (int i = 0; i < g_jm->workersCount; ++i)
            {
              Worker &worker = g_jm->workers[i];
              {
                std::lock_guard<std::mutex> lock(worker.doneTasksMutex);
                for (const JobId &jid : worker.doneTasks)
                  doneTasks.push_back(jid);
                worker.doneTasks.clear();
              }
            }
          }

          {
            // std::lock_guard<std::mutex> lock(g_jm->doneWorkerMutex);
            // g_jm->processedTasksCount += doneTasks.size();
            // g_jm->doneTasksCount -= doneTasks.size();
            ASSERT(g_jm->doneTasksCount >= 0);
          }

          for (const JobId &jid : doneTasks)
            for (int i = 0; i < (int)scheduler.jobIds.size(); ++i)
              if (scheduler.jobIds[i] == jid)
              {
                ASSERT(scheduler.jobs[i].tasksCount >= 1);
                --scheduler.jobs[i].tasksCount;
                break;
              }

          // 5.
          {
            ScopeTimeMS _time(g_stat.scheduler.steps[5]);

            int jobsCount = scheduler.jobs.size();
            eastl::vector<int> doneJobs;
            for (int i = jobsCount - 1; i >= 0; --i)
              if (scheduler.jobs[i].tasksCount == 0)
                doneJobs.push_back(i);

            {
              std::lock_guard<std::mutex> lock(scheduler.doneJobsMutex);
              for (int i : doneJobs)
                scheduler.doneJobs.push_back(scheduler.jobIds[i]);
            }

            for (int i : doneJobs)
            {
              scheduler.jobs.erase(scheduler.jobs.begin() + i);
              scheduler.jobIds.erase(scheduler.jobIds.begin() + i);
              scheduler.jobDependencies.erase(scheduler.jobDependencies.begin() + i);
            }

            if (!scheduler.doneJobs.empty())
              scheduler.doneJobsCV.notify_one();
          }
        }
      }

      // {
      //   std::lock_guard<std::mutex> lock(scheduler.mutex);
      //   scheduler.started = false;
      // }
    }
  }

  JobManager()
  {
    mainThreadId = std::this_thread::get_id();
    mainCpuNo = ::GetCurrentProcessorNumber();

    HANDLE handle = ::GetCurrentThread();
    ::SetThreadAffinityMask(handle, 1 << mainCpuNo);

    workersCount = std::thread::hardware_concurrency() - 1;
    ASSERT(workersCount < (int)workers.size());

    DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
    for (int i = 0; i < workersCount; ++i, ++cpuNo)
    {
      workersThread[i] = eastl::move(std::thread(worker_routine, this, i));
      HANDLE handle = (HANDLE)workersThread[i].native_handle();

      if (cpuNo == mainCpuNo)
        ++cpuNo;

      ::SetThreadAffinityMask(handle, 1 << cpuNo);
    }

    schedulerThread = eastl::move(std::thread(scheduler_routine, this));
    ::SetThreadAffinityMask((HANDLE)schedulerThread.native_handle(), 1 << mainCpuNo);

    doAndWaitAllTasksDone();
  }

  ~JobManager()
  {
    // TODO: stop threads !!!

    scheduler.terminated.store(true);

    {
      std::lock_guard<std::mutex> lock(scheduler.mutex);
      scheduler.started = true;
    }
    scheduler.cv.notify_all();

    if (schedulerThread.joinable())
      schedulerThread.join();

    for (int i = 0; i < workersCount; ++i)
      workers[i].terminated.store(true);

    doAndWaitAllTasksDone();

    for (int i = 0; i < workersCount; ++i)
      if (workersThread[i].joinable())
        workersThread[i].join();
  }

  void addTask(const jobmanager::task_t &task, int from, int count)
  {
    Task &t = tasks.push_back();
    t.task = task;
    t.from = from;
    t.count = count;
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::task_t &task)
  {
    ScopeTimeMS _time(g_stat.jm.createJob);

    uint32_t freeIndex = jobs.size();
  
    static constexpr uint32_t MINIMUM_FREE_INDICES = 1024;
    if (freeJobQueue.size() > MINIMUM_FREE_INDICES)
    {
      freeIndex = freeJobQueue.front();
      freeJobQueue.pop_front();
    }
    else
    {
      jobs.emplace_back();
      jobGenerations.push_back(0);
      jobDependencies.emplace_back();
    }

    ++jobsCount;

    Job &j = jobs[freeIndex];
    j.itemsCount = items_count;
    j.chunkSize = chunk_size;
    j.task = task;

    JobId jid = make_jid(jobGenerations[freeIndex], freeIndex);
    jobsToStart.push_back(jid);

    return jid;
  }

  void deleteJob(const JobId &jid)
  {
    ASSERT(jobGenerations[jid.index] == jid.generation);
    ASSERT(jobs[jid.index].tasksCount == 0);

    --jobsCount;

    jobGenerations[jid.index] = (jobGenerations[jid.index] + 1) % JobId::INDEX_LIMIT;
    freeJobQueue.push_back(jid.index);
  }

  inline bool isDone(const JobId &jid) const
  {
    return jobGenerations[jid.index] != jid.generation;
  }

  void startJobs()
  {
    ScopeTimeMS _time(g_stat.jm.startJobs);

    {
      std::lock_guard<std::mutex> lock(scheduler.futureJobsMutex);
      for (const JobId &jid : jobsToStart)
      {
        scheduler.futureJobIds.push_back(jid);
        scheduler.futureJobs.push_back(jobs[jid.index]);

        // Dependencies already in scheduler or will be added in this loop
        scheduler.futureJobDependencies.push_back(jobDependencies[jid.index]);
      }
    }
    scheduler.cv.notify_one();

    jobsToStart.clear();
  }

  void doAndWaitAllTasksDone()
  {
    ScopeTimeMS _time(g_stat.jm.doAndWaitAllTasksDone);

    startJobs();

    while (jobsCount > 0)
    {
      eastl::vector<JobId> doneJobs;
      {
        ScopeTimeMS _time(g_stat.jm.doneJobsMutex);

        std::unique_lock<std::mutex> lock(scheduler.doneJobsMutex);
        scheduler.doneJobsCV.wait(lock, [&] { return !scheduler.doneJobs.empty(); });

        for (const JobId& jid : scheduler.doneJobs)
          doneJobs.push_back(jid);
        scheduler.doneJobs.clear();
      }

      {
        ScopeTimeMS _time(g_stat.jm.deleteJob);

        for (const JobId& jid : doneJobs)
          deleteJob(jid);
      }
    }

    // const int tasksPerWorker = tasks.size() / workersCount;
    // const int extraTasksPerWorker = tasks.size() % workersCount;

    // for (int i = 0; i < workersCount; ++i)
    //   workers[i].tasks.reserve(tasksPerWorker + extraTasksPerWorker);

    // const int tasksCount = tasks.size();

    // int workerNo = 0;
    // for (int i = 0; i < tasksCount; ++i)
    // {
    //   workers[workerNo].tasks.push_back(tasks[i]);
    //   workerNo = (workerNo + 1) % workersCount;
    // }

    // doneTasksCount = 0;

    // {
    //   std::lock_guard<std::mutex> lock(startWorkerMutex);
    //   for (int i = 0; i < workersCount; ++i)
    //     workers[i].started = true;
    // }
    // startWorkerCV.notify_all();

    // std::unique_lock<std::mutex> lock(doneWorkerMutex);
    // doneWorkerCV.wait(lock, [&](){ return doneTasksCount >= tasksCount; });

    // tasks.clear();
  }
};

static JobManager *g_jm = nullptr;

void jobmanager::init()
{
  g_jm = new JobManager;
}

void jobmanager::release()
{
  delete g_jm;
  g_jm = nullptr;
}

JobId jobmanager::add_job(int items_count, const task_t &task)
{
  return add_job(items_count, 64, task);
}

JobId jobmanager::add_job(int items_count, int chunk_size, const task_t &task)
{
  ASSERT(g_jm != nullptr);
  ASSERT(g_jm->tasks.empty());
  ASSERT(items_count > 0);
  ASSERT(chunk_size > 0);

  if (items_count <= 0)
    return JobId{};

  JobId jid = g_jm->createJob(items_count, chunk_size, task);

  // int itemsLeft = items_count;
  // int tasksCount = (items_count / chunk_size) + ((items_count % chunk_size) ? 1 : 0);
  // g_jm->tasks.reserve(tasksCount);
  // for (int i = 0; i < items_count; i += chunk_size, itemsLeft -= chunk_size)
  //   g_jm->addTask(task, i, eastl::min(chunk_size, itemsLeft));

  return jid;
}

void jobmanager::do_and_wait_all_tasks_done()
{
  ASSERT(g_jm != nullptr);
  g_jm->doAndWaitAllTasksDone();
}

void jobmanager::start_jobs()
{
  ASSERT(g_jm != nullptr);
  g_jm->startJobs();
}

void jobmanager::reset_stat()
{
  g_stat = {};
}

const jobmanager::Stat& jobmanager::get_stat()
{
  return g_stat;
}

#endif