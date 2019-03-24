#include "jobmanager.h"

#include "debug.h"

#include <Windows.h>

#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/bitset.h>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

struct JobManager
{
  struct Task
  {
    jobmanager::task_t task;
    int from;
    int count;
  };

  struct Worker
  {
    std::thread::id threadId;
    uint32_t cpuNo = 0;

    eastl::vector<Task> tasks;
    std::atomic<bool> terminated;
    bool started = false;

    Worker() : terminated(false), started(false)
    {
    }
    Worker(Worker &&assign) : tasks(eastl::move(assign.tasks)), started(assign.started)
    {
      terminated.store(assign.terminated.load());
    }
    Worker(const Worker &assign) : tasks(assign.tasks), started(assign.started)
    {
      terminated.store(assign.terminated.load());
    }
  };

  int workersCount = 0;
  int doneTasksCount = 0;

  std::thread::id mainThreadId;
  uint32_t mainCpuNo = 0;

  std::mutex  startWorkerMutex;
  std::condition_variable startWorkerCV;

  std::mutex  doneWorkerMutex;
  std::condition_variable doneWorkerCV;

  eastl::bitset<8> workersDone;
  eastl::vector<Worker> workers;
  eastl::vector<std::thread> workersThread;

  eastl::vector<Task> tasks;

  static void worker_routine(JobManager *jobs, int worker_id)
  {
    Worker &worker = jobs->workers[worker_id];

    worker.threadId = std::this_thread::get_id();

    while (!worker.terminated.load())
    {
      std::unique_lock<std::mutex> lock(jobs->startWorkerMutex);
      jobs->startWorkerCV.wait(lock, [&](){ return worker.started; });

      worker.cpuNo = ::GetCurrentProcessorNumber();

      for (const auto &t : worker.tasks)
        t.task(t.from, t.count);

      {
        std::lock_guard<std::mutex> lock(jobs->doneWorkerMutex);
        jobs->doneTasksCount += worker.tasks.size();
        worker.started = false;
        worker.tasks.clear();
      }
      jobs->doneWorkerCV.notify_one();
    }
  }

  JobManager()
  {
    mainThreadId = std::this_thread::get_id();
    mainCpuNo = ::GetCurrentProcessorNumber();

    HANDLE handle = ::GetCurrentThread();
    ::SetThreadAffinityMask(handle, 1 << mainCpuNo);

    workersCount = std::thread::hardware_concurrency() - 1;
    workers.resize(workersCount);
    workersThread.resize(workersCount);

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

  void addTask(const jobmanager::task_t &task, int from, int count)
  {
    Task &t = tasks.push_back();
    t.task = task;
    t.from = from;
    t.count = count;
  }

  void doAndWaitAllTasksDone()
  {
    const int tasksPerWorker = tasks.size() / workersCount;
    const int extraTasksPerWorker = tasks.size() % workersCount;

    doneTasksCount = 0;

    for (auto &w : workers)
      w.tasks.reserve(tasksPerWorker + extraTasksPerWorker);

    const int tasksCount = tasks.size();

    int workerNo = 0;
    for (int i = 0; i < tasksCount; ++i)
    {
      workers[workerNo].tasks.push_back(tasks[i]);
      workerNo = (workerNo + 1) % workersCount;
    }

    {
      std::lock_guard<std::mutex> lock(startWorkerMutex);
      for (auto &w : workers)
        w.started = true;
    }
    startWorkerCV.notify_all();

    std::unique_lock<std::mutex> lock(doneWorkerMutex);
    doneWorkerCV.wait(lock, [&](){ return doneTasksCount >= tasksCount; });

    tasks.clear();
  }
};

static JobManager *jobs = nullptr;

void jobmanager::init()
{
  jobs = new JobManager;
}

void jobmanager::release()
{
  delete jobs;
  jobs = nullptr;
}

void jobmanager::add_job(int chunk_size, int items_count, const task_t &task)
{
  ASSERT(jobs != nullptr);
  ASSERT(jobs->tasks.empty());

  if (items_count <= 0)
    return;

  int itemsLeft = items_count;

  chunk_size = items_count / jobs->workersCount;
  chunk_size = chunk_size == 0 ? items_count : chunk_size;

  int tasksCount = (items_count / chunk_size) + ((items_count % chunk_size) ? 1 : 0);
  jobs->tasks.reserve(tasksCount);
  for (int i = 0; i < items_count; i += chunk_size, itemsLeft -= chunk_size)
    jobs->addTask(task, i, eastl::min(chunk_size, itemsLeft));
}

void jobmanager::do_and_wait_all_tasks_done()
{
  ASSERT(jobs != nullptr);
  jobs->doAndWaitAllTasksDone();
}