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

// TODO: Optimize memory usage with containers
// TODO: More optimizations

#define USE_OS_EVENTS 1

using JobId = jobmanager::JobId;
using DependencyList = jobmanager::DependencyList;

static inline JobId make_jid(uint16_t gen, uint32_t index)
{
  return JobId(((uint32_t)gen << JobId::INDEX_BITS | index));
}

static jobmanager::Stat g_stat;

static std::mutex g_output_mutex;
static eastl::vector<eastl::string> g_output_buffer;

#define THREAD_LOG(fmt, ...)\
{\
  std::lock_guard<std::mutex> lock(g_output_mutex);\
  g_output_buffer.emplace_back(eastl::string::CtorSprintf(), fmt, __VA_ARGS__);\
}\

#define THREAD_LOG_FLUSH \
{\
  std::lock_guard<std::mutex> lock(g_output_mutex);\
  g_output_buffer.clear();\
}\

// Comment this in case of debug
#undef THREAD_LOG
#undef THREAD_LOG_FLUSH
#define THREAD_LOG(...)
#define THREAD_LOG_FLUSH

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
#define SCOPE_TIME_N(v, n) ScopeTimeMS _time_##n(v);
#else
#define SCOPE_TIME(...)
#define SCOPE_TIME_N(...)
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
    std::mutex startMutex;

    #if USE_OS_EVENTS
    HANDLE startEvent = NULL;
    #else
    std::condition_variable startCV;
    #endif

    eastl::vector<Task> tasks;
    eastl::vector<jobmanager::callback_t> jobCallbacks;

    std::atomic<bool> terminated = false;
    bool started = false;
  };

  struct Scheduler
  {
    std::mutex startMutex;

    #if USE_OS_EVENTS
    HANDLE startEvent = NULL;
    #else
    std::condition_variable startCV;
    #endif

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

  eastl::array<Worker, 16> workers;
  eastl::array<std::thread, 16> workersThread;
  eastl::bitset<16> startedWorkers;

  std::mutex doneWorkersMutex;
  eastl::bitset<16> doneWorkers;
  #if USE_OS_EVENTS
  HANDLE doneWorkersEvent = NULL;
  #else
  std::condition_variable doneWorkersCV;
  #endif

  std::thread schedulerThread;
  Scheduler scheduler;

  eastl::deque<uint32_t> freeJobQueue;

  int jobsCount = 0;
  eastl::vector<Job> jobs;
  eastl::vector<uint8_t> jobGenerations;
  eastl::vector<DependencyList> jobDependencies;

  eastl::vector<JobId> jobsToStart;
  eastl::vector<JobId> jobsToRemove;


  std::mutex doneJobMutex;
  #if USE_OS_EVENTS
  HANDLE doneJobEvent = NULL;
  #else
  std::condition_variable doneJobCV;
  #endif

  std::mutex currentTasksMutex;
  eastl::queue<eastl::vector<Task>> tasksQueue;
  eastl::queue<eastl::vector<JobInQueue>> jobsQueue;
  eastl::queue<eastl::vector<JobId>> barriersQueue;

  eastl::vector<Task> currentTasks;
  eastl::vector<JobInQueue> currentJobs;
  eastl::vector<JobId> currentBarriers;

  static void worker_routine(JobManager *jm, int worker_id)
  {
    Worker &worker = jm->workers[worker_id];

    while (true)
    {
      {
        #if USE_OS_EVENTS
        ::WaitForSingleObjectEx(worker.startEvent, INFINITE, TRUE);
        #else
        std::unique_lock<std::mutex> lock(worker.startMutex);
        worker.startCV.wait(lock, [&](){ return worker.started; });
        #endif

        ASSERT(worker.started);

        if (worker.terminated.load())
          return;
      }

      THREAD_LOG("Worker[%d]: start", worker_id);

      SCOPE_TIME(g_stat.workers.total[worker_id]);

      {
        SCOPE_TIME(g_stat.workers.task[worker_id]);
        for (const Task &task : worker.tasks)
          worker.jobCallbacks[task.jobIdx](task.from, task.count);
      }

      {
        std::lock_guard<std::mutex> lock(worker.startMutex);
        worker.started = false;
      }

      THREAD_LOG("Worker[%d]: done", worker_id);

      {
        SCOPE_TIME(g_stat.workers.done[worker_id]);
        std::lock_guard<std::mutex> lock(jm->doneWorkersMutex);
        jm->doneWorkers.set(worker_id);
      }
      #if USE_OS_EVENTS
      ::SetEvent(jm->doneWorkersEvent);
      #else
      jm->doneWorkersCV.notify_one();
      #endif
    }
  }

  static void scheduler_routine(JobManager *jm)
  {
    Scheduler &scheduler = jm->scheduler;

    while (true)
    {
      {
        #if USE_OS_EVENTS
        if (!scheduler.started)
          ::WaitForSingleObjectEx(scheduler.startEvent, INFINITE, TRUE);
        #else
        std::unique_lock<std::mutex> lock(scheduler.startMutex);
        scheduler.startCV.wait(lock, [&](){ return scheduler.started; });
        #endif

        if (scheduler.terminated.load())
          return;
      }

      SCOPE_TIME(g_stat.scheduler.total);

      bool popFromQueue = false;
      if (!jm->startedWorkers.any())
      {
        SCOPE_TIME_N(g_stat.scheduler.popQueueTotal, 1);
        std::lock_guard<std::mutex> lock(jm->currentTasksMutex);
        SCOPE_TIME_N(g_stat.scheduler.popQueue, 2);
        popFromQueue =
          jm->currentJobs.empty() &&
          jm->currentTasks.empty() &&
          jm->currentBarriers.empty() &&
          !jm->jobsQueue.empty() &&
          !jm->barriersQueue.empty() &&
          !jm->tasksQueue.empty();
        if (popFromQueue)
        {
          THREAD_LOG("Scheduler: popFromQueue");

          jm->currentJobs = eastl::move(jm->jobsQueue.front());
          jm->currentTasks = eastl::move(jm->tasksQueue.front());
          jm->currentBarriers = eastl::move(jm->barriersQueue.front());
          jm->jobsQueue.pop();
          jm->barriersQueue.pop();
          jm->tasksQueue.pop();
        }
      }

      if (popFromQueue)
        for (int i = 0; i < jm->workersCount; ++i)
        {
          #ifdef _DEBUG
          {
            std::lock_guard<std::mutex> lock(jm->workers[i].startMutex);
            ASSERT(!jm->workers[i].started);
          }
          #endif
          jm->workers[i].jobCallbacks.clear();
          jm->workers[i].jobCallbacks.reserve(jm->currentJobs.size());
          for (const auto &job : jm->currentJobs)
            jm->workers[i].jobCallbacks.push_back(job.callback);
        }

      const int currentTasksCount = jm->currentTasks.size();

      {
        SCOPE_TIME(g_stat.scheduler.assignTasks);

        const int tasksPerWorker = (currentTasksCount + (jm->workersCount - (currentTasksCount % jm->workersCount))) / jm->workersCount;

        auto currentTasksIter = jm->currentTasks.begin();
        int tasksLeft = currentTasksCount;

        for (int i = 0; i < jm->workersCount && tasksLeft > 0; ++i, tasksLeft -= tasksPerWorker, currentTasksIter += tasksPerWorker)
        {
          int tasksCount = eastl::min(tasksPerWorker, tasksLeft);

          eastl::vector<Task> squashedTasks;
          squashedTasks.reserve(tasksCount);

          if (tasksCount > 0)
            squashedTasks.push_back(*currentTasksIter);

          auto lastJobId = currentTasksIter->jid;
          for (auto taskIt = currentTasksIter + 1, taskEndIt = currentTasksIter + tasksCount; taskIt != taskEndIt; ++taskIt)
            if (taskIt->jid == lastJobId)
            {
              Task &curTask = squashedTasks.back();
              ASSERT(taskIt->from == curTask.from + curTask.count);
              curTask.count += taskIt->count;
            }
            else
            {
              lastJobId = taskIt->jid;
              squashedTasks.push_back(*taskIt);
            }

          for (const auto &task : squashedTasks)
            for (auto &job : jm->currentJobs)
              if (job.jid == task.jid)
              {
                ++job.tasksCount;
                break;
              }

          for (const auto &task : squashedTasks)
            THREAD_LOG("Scheduler: assign task from [%d] to [%d]", task.jid, i);

          jm->workers[i].tasks = eastl::move(squashedTasks);
        }

        jm->currentTasks.clear();
      }

      if (currentTasksCount > 0)
      {
        SCOPE_TIME(g_stat.scheduler.startWorkers);

        for (int i = 0; i < jm->workersCount; ++i)
          if (!jm->workers[i].tasks.empty())
          {
            THREAD_LOG("Scheduler: start [%d]", i);

            jm->startedWorkers.set(i);
            {
              std::lock_guard<std::mutex> lock(jm->workers[i].startMutex);
              jm->workers[i].started = true;
            }
            #if USE_OS_EVENTS
            ::SetEvent(jm->workers[i].startEvent);
            #else
            jm->workers[i].startCV.notify_one();
            #endif
          }
      }

      eastl::array<eastl::vector<Task>, 16> doneTasks;
      {
        SCOPE_TIME(g_stat.scheduler.doneTasksTotal);

        eastl::bitset<16> doneWorkers;

        if (jm->startedWorkers.any())
        {
          #if USE_OS_EVENTS
          ::WaitForSingleObjectEx(jm->doneWorkersEvent, INFINITE, TRUE);
          #else
          std::unique_lock<std::mutex> lock(jm->doneWorkersMutex);
          jm->doneWorkersCV.wait(lock, [&](){ return jm->doneWorkers.any(); });
          #endif

          SCOPE_TIME(g_stat.scheduler.doneTasks);
          {
            #if USE_OS_EVENTS
            std::lock_guard<std::mutex> lock(jm->doneWorkersMutex);
            #endif
            doneWorkers = jm->doneWorkers;
            jm->doneWorkers.reset();
          }
        }

        for (int i = 0; i < jm->workersCount; ++i)
          if (doneWorkers[i])
          {
            THREAD_LOG("Scheduler: Worker [%d] done", i);

            jm->startedWorkers.reset(i);
            doneTasks[i] = eastl::move(jm->workers[i].tasks);
            jm->workers[i].tasks.clear();
          }
      }

      {
        SCOPE_TIME(g_stat.scheduler.finalizeTasks);

        eastl::vector<JobId> jobsToRemove;
        jobsToRemove.reserve(jm->currentJobs.size() + jm->currentBarriers.size());

        if (!jm->currentBarriers.empty())
        {
          jobsToRemove.resize(jm->currentBarriers.size());
          eastl::uninitialized_copy_n(jm->currentBarriers.begin(), jm->currentBarriers.size(), jobsToRemove.begin());
          jm->currentBarriers.clear();
        }

        for (int workerIdx = 0; workerIdx < jm->workersCount; ++workerIdx)
          for (const Task &task : doneTasks[workerIdx])
          {
            SCOPE_TIME(g_stat.scheduler.finalizeJobsToRemove);

            std::lock_guard<std::mutex> lock(jm->currentTasksMutex);
            for (int i = jm->currentJobs.size() - 1; i >= 0; --i)
              if (task.jid == jm->currentJobs[i].jid)
              {
                THREAD_LOG("Scheduler: worker[%d] done task from [%d] left [%d]", workerIdx, task.jid.handle, jm->currentJobs[i].tasksCount - 1);
                if (--jm->currentJobs[i].tasksCount == 0)
                {
                  jm->currentJobs.erase(jm->currentJobs.begin() + i);
                  jobsToRemove.push_back(task.jid);
                }
                break;
              }
          }

        if (!jobsToRemove.empty())
        {
          SCOPE_TIME_N(g_stat.scheduler.finalizeDeleteJobsTotal, 1);
          {
            std::lock_guard<std::mutex> lock(jm->doneJobMutex);
            SCOPE_TIME_N(g_stat.scheduler.finalizeDeleteJobs, 2);
            const size_t offset = jm->jobsToRemove.size();
            jm->jobsToRemove.resize(offset + jobsToRemove.size());
            eastl::uninitialized_copy_n(jobsToRemove.begin(), jobsToRemove.size(), jm->jobsToRemove.begin() + offset);
          }
          #if USE_OS_EVENTS
          ::SetEvent(jm->doneJobEvent);
          #else
          jm->doneJobCV.notify_one();
          #endif
        }

        {
          std::lock_guard<std::mutex> lock(jm->currentTasksMutex);
          if (jm->jobsQueue.empty() && jm->barriersQueue.empty() && !jm->startedWorkers.any())
          {
            std::lock_guard<std::mutex> lock(scheduler.startMutex);
            scheduler.started = false;
          }
        }
      }
    }
  }

  JobManager()
  {
    mainThreadId = std::this_thread::get_id();
    mainCpuNo = ::GetCurrentProcessorNumber();

    HANDLE handle = ::GetCurrentThread();
    ::SetThreadAffinityMask(handle, 1 << mainCpuNo);

    doneWorkers.reset();
    startedWorkers.reset();

    workersCount = std::thread::hardware_concurrency() - 1;
    ASSERT(workersCount < (int)workers.size());

    DWORD cpuNo = mainCpuNo == 0 ? 1 : 0;
    for (int i = 0; i < workersCount; ++i, ++cpuNo)
    {
      #if USE_OS_EVENTS
      workers[i].startEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
      #endif
      workersThread[i] = eastl::move(std::thread(worker_routine, this, i));
      HANDLE handle = (HANDLE)workersThread[i].native_handle();

      if (cpuNo == mainCpuNo)
        ++cpuNo;

      ::SetThreadAffinityMask(handle, 1 << cpuNo);
    }

    #if USE_OS_EVENTS
    doneWorkersEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    doneJobEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    scheduler.startEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    #endif

    schedulerThread = eastl::move(std::thread(scheduler_routine, this));

    // This reduce wait time on doneJobMutex
    ::SetThreadAffinityMask((HANDLE)schedulerThread.native_handle(), 1 << ((mainCpuNo + 1) % workersCount));
    // ::SetThreadAffinityMask((HANDLE)schedulerThread.native_handle(), 1 << mainCpuNo);

    doAndWaitAllTasksDone();
  }

  ~JobManager()
  {
    doAndWaitAllTasksDone();

    scheduler.terminated.store(true);

    {
      std::lock_guard<std::mutex> lock(scheduler.startMutex);
      scheduler.started = true;
    }
    #if USE_OS_EVENTS
    ::SetEvent(scheduler.startEvent);
    #else
    scheduler.startCV.notify_one();
    #endif

    if (schedulerThread.joinable())
      schedulerThread.join();

    for (int i = 0; i < workersCount; ++i)
      workers[i].terminated.store(true);

    for (int i = 0; i < workersCount; ++i)
    {
      {
        std::lock_guard<std::mutex> lock(workers[i].startMutex);
        workers[i].started = true;
      }
      #if USE_OS_EVENTS
      ::SetEvent(workers[i].startEvent);
      #else
      workers[i].startCV.notify_one();
      #endif
    }

    for (int i = 0; i < workersCount; ++i)
    {
      if (workersThread[i].joinable())
        workersThread[i].join();
    }

    #if USE_OS_EVENTS
    for (int i = 0; i < workersCount; ++i)
      ::CloseHandle(workers[i].startEvent);

    ::CloseHandle(doneWorkersEvent);
    #endif
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::callback_t &task, const jobmanager::DependencyList &dependencies)
  {
    jobmanager::DependencyList tmpDependencies(dependencies);
    return createJob(items_count, chunk_size, task, eastl::move(tmpDependencies));
  }

  JobId createJob(int items_count, int chunk_size, const jobmanager::callback_t &task, jobmanager::DependencyList &&dependencies = {})
  {
    std::lock_guard<std::mutex> lock(doneJobMutex);

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
    jobsToStart.push_back(jid);

    THREAD_LOG("createJob: %d", jid.handle);

    return jid;
  }

  void deleteJob(const JobId &jid)
  {
    ASSERT(jobGenerations[jid.index] == jid.generation);

    THREAD_LOG("deleteJob: %d", jid.handle);

    --jobsCount;

    jobs[jid.index].queued = false;
    jobGenerations[jid.index] = uint16_t(jobGenerations[jid.index] + 1) % JobId::GENERATION_LIMIT;
    freeJobQueue.push_back(jid.index);
  }

  inline bool isDone(const JobId &jid) const
  {
    return jobGenerations[jid.index] != jid.generation;
  }

  void startJobs()
  {
    if (jobsToStart.empty())
      return;

    SCOPE_TIME(g_stat.jm.startJobs);

    while (!jobsToStart.empty())
    {
      SCOPE_TIME(g_stat.jm.startJobQueues);

      eastl::vector<Task> futureTasks;
      eastl::vector<JobInQueue> futureJobs;
      eastl::vector<JobId> futureBarriers;

      futureJobs.reserve(jobsToStart.size());

      for (int jobIdx = jobsToStart.size() - 1; jobIdx >= 0; --jobIdx)
      {
        JobId jid = jobsToStart[jobIdx];

        bool canStart = true;
        Job job;

        {
          std::lock_guard<std::mutex> lock(doneJobMutex);

          job = jobs[jid.index];

          const DependencyList &deps = jobDependencies[jid.index];
          for (const JobId &depJid : deps)
            if (!isDone(depJid) && !jobs[depJid.index].queued)
            {
              canStart = false;
              break;
            }
        }

        if (canStart)
        {
          jobsToStart.erase(jobsToStart.begin() + jobIdx);

          if (job.task)
          {
            int itemsLeft = job.itemsCount;
            int tasksCount = (job.itemsCount / job.chunkSize) + ((job.itemsCount % job.chunkSize) ? 1 : 0);

            futureJobs.push_back({jid, job.task, 0});
            futureTasks.reserve(futureTasks.size() + tasksCount);

            for (int i = 0; i < job.itemsCount; i += job.chunkSize, itemsLeft -= job.chunkSize)
            {
              Task t;
              t.jid = jid;
              t.from = i;
              t.count = eastl::min(job.chunkSize, itemsLeft);
              t.jobIdx = futureJobs.size() - 1;
              futureTasks.push_back(eastl::move(t));
            }
          }
          else
            futureBarriers.push_back(jid);
        }
      }

      {
        std::lock_guard<std::mutex> lock(doneJobMutex);
        for (const auto &job : futureJobs)
          jobs[job.jid.index].queued = true;
        for (const auto &jid : futureBarriers)
          jobs[jid.index].queued = true;
      }

      ASSERT(!futureTasks.empty() || !futureBarriers.empty());

      {
        std::lock_guard<std::mutex> lock(currentTasksMutex);
        tasksQueue.push(eastl::move(futureTasks));
        jobsQueue.push(eastl::move(futureJobs));
        barriersQueue.push(eastl::move(futureBarriers));
      }
    }

    {
      std::lock_guard<std::mutex> lock(scheduler.startMutex);
      scheduler.started = true;
    }
    #if USE_OS_EVENTS
    ::SetEvent(scheduler.startEvent);
    #else
    scheduler.startCV.notify_one();
    #endif
  }

  void doAndWaitAllTasksDone()
  {
    SCOPE_TIME(g_stat.jm.doAndWaitAllTasksDone);

    startJobs();

    while (jobsCount > 0)
    {
      SCOPE_TIME(g_stat.jm.doneJobsMutex);
      #if USE_OS_EVENTS
      ::WaitForSingleObjectEx(doneJobEvent, INFINITE, TRUE);
      #else
      std::unique_lock<std::mutex> lock(doneJobMutex);
      doneJobCV.wait(lock, [&](){ return !jobsToRemove.empty(); });
      #endif

      {
        #if USE_OS_EVENTS
        std::lock_guard<std::mutex> lock(doneJobMutex);
        #endif
        for (const JobId &jid : jobsToRemove)
          deleteJob(jid);
      }

      jobsToRemove.clear();
    }

    THREAD_LOG_FLUSH;
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

JobId jobmanager::add_job(const jobmanager::DependencyList &dependencies)
{
  ASSERT(g_jm != nullptr);
  return g_jm->createJob(1, 1, nullptr, eastl::move(dependencies));
}

JobId jobmanager::add_job(jobmanager::DependencyList &&dependencies)
{
  ASSERT(g_jm != nullptr);
  return g_jm->createJob(1, 1, nullptr, eastl::move(dependencies));
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