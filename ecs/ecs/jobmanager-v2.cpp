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
    eastl::vector<Task> tasks;

    std::atomic<bool> terminated = false;
    bool started = false;
  };

  int workersCount = 0;

  std::thread::id mainThreadId;
  uint32_t mainCpuNo = 0;

  std::mutex startWorkerMutex;
  std::condition_variable startWorkerCV;

  std::mutex doneJobMutex;
  std::condition_variable doneJobCV;

  eastl::array<Worker, 16> workers;
  eastl::array<std::thread, 16> workersThread;

  eastl::deque<uint32_t> freeJobQueue;

  int jobsCount = 0;
  eastl::vector<Job> jobs;
  eastl::vector<uint8_t> jobGenerations;
  eastl::vector<eastl::vector<JobId>> jobDependencies;

  eastl::vector<JobId> jobsToStart;

  struct JobInQueue
  {
    JobId jid;
    int tasksCount = 0;
  };

  std::mutex currentTasksMutex;
  eastl::queue<eastl::queue<Task>> tasksQueue;
  eastl::queue<eastl::vector<JobInQueue>> jobsQueue;

  eastl::queue<Task> currentTasks;
  eastl::vector<JobInQueue> currentJobs;

  static void worker_routine(JobManager *g_jm, int worker_id)
  {
    Worker &worker = g_jm->workers[worker_id];

    while (!worker.terminated.load())
    {
      {
        std::unique_lock<std::mutex> lock(g_jm->startWorkerMutex);
        g_jm->startWorkerCV.wait(lock, [&](){ return worker.started; });
      }

      ScopeTimeMS _time(g_stat.workers.total[worker_id]);

      {
        ScopeTimeMS _time1(g_stat.workers.currentTasksMutexTotal[worker_id]);
        std::lock_guard<std::mutex> lock(g_jm->currentTasksMutex);
        ScopeTimeMS _time2(g_stat.workers.currentTasksMutex[worker_id]);

        for (const Task &task : worker.tasks)
          for (int i = g_jm->currentJobs.size() - 1; i >= 0; --i)
            if (task.jid == g_jm->currentJobs[i].jid)
            {
              if (--g_jm->currentJobs[i].tasksCount == 0)
              {
                g_jm->currentJobs.erase(g_jm->currentJobs.begin() + i);

                {
                  std::lock_guard<std::mutex> lock(g_jm->doneJobMutex);
                  g_jm->deleteJob(task.jid);
                }
                g_jm->doneJobCV.notify_one();
              }
              break;
            }

        if (g_jm->currentJobs.empty() && g_jm->currentTasks.empty() && !g_jm->jobsQueue.empty() && !g_jm->tasksQueue.empty())
        {
          g_jm->currentJobs = eastl::move(g_jm->jobsQueue.front());
          g_jm->currentTasks = eastl::move(g_jm->tasksQueue.front());
          g_jm->jobsQueue.pop();
          g_jm->tasksQueue.pop();
        }

        worker.tasks.clear();

        for (int i = 0, sz = eastl::min(8, (int)g_jm->currentTasks.size()); i < sz; ++i)
        {
          worker.tasks.push_back(eastl::move(g_jm->currentTasks.front()));
          g_jm->currentTasks.pop();
        }
      }

      {
        ScopeTimeMS _time(g_stat.workers.task[worker_id]);
        for (const Task &task : worker.tasks)
          task.task(task.from, task.count);
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
    // TODO: stop threads !!!
    for (int i = 0; i < workersCount; ++i)
      workers[i].terminated.store(true);

    doAndWaitAllTasksDone();

    for (int i = 0; i < workersCount; ++i)
      if (workersThread[i].joinable())
        workersThread[i].join();
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::task_t &task)
  {
    std::lock_guard<std::mutex> lock(doneJobMutex);

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
    // ASSERT(jobs[jid.index].tasksCount == 0);

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

    eastl::queue<Task> futureTasks;
    eastl::vector<JobInQueue> futureJobs;

    for (const JobId &jid : jobsToStart)
    {
      const Job &job = jobs[jid.index];

      int itemsLeft = job.itemsCount;
      int tasksCount = (job.itemsCount / job.chunkSize) + ((job.itemsCount % job.chunkSize) ? 1 : 0);
      futureJobs.push_back({jid, tasksCount});
      for (int i = 0; i < job.itemsCount; i += job.chunkSize, itemsLeft -= job.chunkSize)
      {
        Task t;
        t.jid = jid;
        t.task = job.task;
        t.from = i;
        t.count = eastl::min(job.chunkSize, itemsLeft);
        futureTasks.push(eastl::move(t));
      }
    }

    if (!futureTasks.empty())
    {
      {
        std::lock_guard<std::mutex> lock(currentTasksMutex);
        tasksQueue.push(eastl::move(futureTasks));
        jobsQueue.push(eastl::move(futureJobs));
      }

      {
        std::lock_guard<std::mutex> lock(startWorkerMutex);
        for (int i = 0; i < workersCount; ++i)
          workers[i].started = true;
      }
      startWorkerCV.notify_all();
    }

    jobsToStart.clear();
  }

  void doAndWaitAllTasksDone()
  {
    ScopeTimeMS _time(g_stat.jm.doAndWaitAllTasksDone);

    startJobs();

    while (jobsCount > 0)
    {
      {
        ScopeTimeMS _time(g_stat.jm.doneJobsMutex);
        std::unique_lock<std::mutex> lock(doneJobMutex);
        doneJobCV.wait(lock, [&](){ return jobsCount == 0; });
      }
      // {
      //   ScopeTimeMS _time(g_stat.jm.doneJobsMutex);

      //   std::unique_lock<std::mutex> lock(doneJobsMutex);
      //   scheduler.doneJobsCV.wait(lock, [&] { return !scheduler.doneJobs.empty(); });

      //   for (const JobId& jid : scheduler.doneJobs)
      //     doneJobs.push_back(jid);
      //   scheduler.doneJobs.clear();
      // }
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
  ASSERT(g_jm->tasksQueue.empty());
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