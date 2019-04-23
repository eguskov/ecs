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
using DependencyList = jobmanager::DependencyList;

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
using ScopeTimeUS = ScopeTime<std::micro>;

#ifdef _DEBUG
#define SCOPE_TIME(v) ScopeTimeMS _time(v);
#else
#define SCOPE_TIME(...)
#endif

struct JobManager
{
  struct Job
  {
    int itemsCount = 0;
    int chunkSize = 0;

    jobmanager::callback_t task;

    bool queued = false;
  };

  struct Task
  {
    JobId jid;
    int from;
    int count;
    int jobIdx;
  };

  struct Worker
  {
    std::mutex tasksMutex;
    eastl::vector<Task> tasks;
    eastl::vector<jobmanager::callback_t> jobCallbacks;

    std::atomic<bool> terminated = false;
    bool started = false;
  };

  struct JobInQueue
  {
    JobId jid;
    jobmanager::callback_t callback;
    int tasksCount = 0;
  };

  int workersCount = 0;

  std::thread::id mainThreadId;
  uint32_t mainCpuNo = 0;

  std::mutex startWorkerMutex;
  std::condition_variable startWorkerCV;

  eastl::array<Worker, 16> workers;
  eastl::array<std::thread, 16> workersThread;

  eastl::deque<uint32_t> freeJobQueue;

  int jobsCount = 0;
  eastl::vector<Job> jobs;
  eastl::vector<uint8_t> jobGenerations;
  eastl::vector<DependencyList> jobDependencies;

  eastl::queue<JobId> jobsToStart;

  std::mutex currentTasksMutex;
  std::condition_variable doneJobCV;
  eastl::queue<eastl::queue<Task>> tasksQueue;
  eastl::queue<eastl::vector<JobInQueue>> jobsQueue;

  eastl::queue<Task> currentTasks;
  eastl::vector<JobInQueue> currentJobs;

  static void worker_routine(JobManager *g_jm, int worker_id)
  {
    Worker &worker = g_jm->workers[worker_id];

    while (true)
    {
      {
        std::unique_lock<std::mutex> lock(g_jm->startWorkerMutex);
        g_jm->startWorkerCV.wait(lock, [&](){ return worker.started; });

        if (worker.terminated.load())
          return;
      }

      SCOPE_TIME(g_stat.workers.total[worker_id]);

      {
        SCOPE_TIME(g_stat.workers.task[worker_id]);

        std::lock_guard<std::mutex> lock(worker.tasksMutex);
        for (const Task &task : worker.tasks)
          worker.jobCallbacks[task.jobIdx](task.from, task.count);
      }

      {
        SCOPE_TIME(g_stat.workers.finalize[worker_id]);

        std::lock_guard<std::mutex> lock(g_jm->currentTasksMutex);

        {
          std::lock_guard<std::mutex> lock(worker.tasksMutex);
          for (const Task &task : worker.tasks)
            for (int i = g_jm->currentJobs.size() - 1; i >= 0; --i)
              if (task.jid == g_jm->currentJobs[i].jid)
              {
                if (--g_jm->currentJobs[i].tasksCount == 0)
                {
                  g_jm->currentJobs.erase(g_jm->currentJobs.begin() + i);
                  g_jm->deleteJob(task.jid);
                  g_jm->doneJobCV.notify_one();
                }
                break;
              }

          worker.tasks.clear();
        }

        if (g_jm->currentJobs.empty() && g_jm->currentTasks.empty() && !g_jm->jobsQueue.empty() && !g_jm->tasksQueue.empty())
        {
          g_jm->currentJobs = eastl::move(g_jm->jobsQueue.front());
          g_jm->currentTasks = eastl::move(g_jm->tasksQueue.front());
          g_jm->jobsQueue.pop();
          g_jm->tasksQueue.pop();
        }
      }

      if (worker.tasks.empty())
      {
        std::lock_guard<std::mutex> lock(g_jm->startWorkerMutex);
        worker.started = false;
      }
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

    doAndWaitAllTasksDone();
  }

  ~JobManager()
  {
    for (int i = 0; i < workersCount; ++i)
      workers[i].terminated.store(true);

    doAndWaitAllTasksDone();

    for (int i = 0; i < workersCount; ++i)
      if (workersThread[i].joinable())
        workersThread[i].join();
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::callback_t &task, const jobmanager::DependencyList &dependencies)
  {
    jobmanager::DependencyList tmpDependencies(dependencies);
    return createJob(items_count, chunk_size, task, eastl::move(tmpDependencies));
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::callback_t &task, jobmanager::DependencyList &&dependencies = {})
  {
    std::lock_guard<std::mutex> lock(currentTasksMutex);

    SCOPE_TIME(g_stat.jm.createJob);

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
      jobDependencies.push_back(eastl::move(dependencies));
    }

    ++jobsCount;

    Job &j = jobs[freeIndex];
    j.itemsCount = items_count;
    j.chunkSize = chunk_size;
    j.task = task;

    JobId jid = make_jid(jobGenerations[freeIndex], freeIndex);
    jobsToStart.push(jid);

    return jid;
  }

  void deleteJob(const JobId &jid)
  {
    ASSERT(jobGenerations[jid.index] == jid.generation);

    --jobsCount;

    jobs[jid.index].queued = false;
    jobGenerations[jid.index] = (jobGenerations[jid.index] + 1) % JobId::INDEX_LIMIT;
    freeJobQueue.push_back(jid.index);
  }

  inline bool isDone(const JobId &jid) const
  {
    return jobGenerations[jid.index] != jid.generation;
  }

  void startJobs()
  {
    SCOPE_TIME(g_stat.jm.startJobs);

    while (!jobsToStart.empty())
    {
      eastl::queue<Task> futureTasks;
      eastl::vector<JobInQueue> futureJobs;

      const JobId &jid = jobsToStart.front();

      Job &job = jobs[jid.index];
      const DependencyList &deps = jobDependencies[jid.index];

      bool canStart = true;
      for (const JobId &depJid : deps)
        if (!isDone(depJid) && !jobs[depJid.index].queued)
        {
          canStart = false;
          break;
        }

      if (canStart)
      {
        job.queued = true;

        int itemsLeft = job.itemsCount;
        int tasksCount = (job.itemsCount / job.chunkSize) + ((job.itemsCount % job.chunkSize) ? 1 : 0);

        futureJobs.push_back({jid, job.task, tasksCount});

        for (int i = 0; i < job.itemsCount; i += job.chunkSize, itemsLeft -= job.chunkSize)
        {
          Task t;
          t.jid = jid;
          t.from = i;
          t.count = eastl::min(job.chunkSize, itemsLeft);
          t.jobIdx = futureJobs.size() - 1;
          futureTasks.push(eastl::move(t));
        }

        jobsToStart.pop();
      }
    }

    if (!futureTasks.empty())
    {
      std::lock_guard<std::mutex> lock(currentTasksMutex);
      tasksQueue.push(eastl::move(futureTasks));
      jobsQueue.push(eastl::move(futureJobs));

      if (currentJobs.empty() && currentTasks.empty() && !jobsQueue.empty() && !tasksQueue.empty())
      {
        currentJobs = eastl::move(jobsQueue.front());
        currentTasks = eastl::move(tasksQueue.front());
        jobsQueue.pop();
        tasksQueue.pop();

        for (auto &w : workers)
        {
          std::lock_guard<std::mutex> lock(w.tasksMutex);
          w.jobCallbacks.clear();
          w.jobCallbacks.reserve(currentJobs.size());
          for (const auto &job : currentJobs)
            w.jobCallbacks.push_back(job.callback);
        }
      }

      int workerNo = 0;
      while (!currentTasks.empty())
      {
        {
          std::lock_guard<std::mutex> lock(workers[workerNo].tasksMutex);
          workers[workerNo].tasks.push_back(eastl::move(currentTasks.front()));
        }
        currentTasks.pop();
        workerNo = (workerNo + 1) % workersCount;
      }
    }

    {
      std::lock_guard<std::mutex> lock(startWorkerMutex);
      for (int i = 0; i < workersCount; ++i)
        workers[i].started = true;
    }
    startWorkerCV.notify_all();

    jobsToStart.clear();
  }

  void doAndWaitAllTasksDone()
  {
    SCOPE_TIME(g_stat.jm.doAndWaitAllTasksDone);

    startJobs();

    {
      SCOPE_TIME(g_stat.jm.doneJobsMutex);
      std::unique_lock<std::mutex> lock(currentTasksMutex);
      doneJobCV.wait(lock, [&](){ return jobsCount == 0; });
    }
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

JobId jobmanager::add_job(int items_count, const callback_t &task)
{
  return add_job(items_count, 64, task);
}

JobId jobmanager::add_job(int items_count, int chunk_size, const callback_t &task)
{
  ASSERT(g_jm != nullptr);
  ASSERT(g_jm->tasksQueue.empty());
  ASSERT(items_count > 0);
  ASSERT(chunk_size > 0);

  if (items_count <= 0)
    return JobId{};

  return g_jm->createJob(items_count, chunk_size, task);
}

JobId jobmanager::add_job(const jobmanager::DependencyList &dependencies, int items_count, int chunk_size, const callback_t &task)
{
  ASSERT(g_jm != nullptr);
  return g_jm->createJob(items_count, chunk_size, task, dependencies);
}

JobId jobmanager::add_job(jobmanager::DependencyList &&dependencies, int items_count, int chunk_size, const callback_t &task)
{
  ASSERT(g_jm != nullptr);
  return g_jm->createJob(items_count, chunk_size, task, eastl::move(dependencies));
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