#include "jobmanager.h"

#include "debug.h"

#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <thread>
#include <atomic>

struct JobManager
{
  int workersCount = 0;

  struct Task
  {
    jobmanager::task_t task;
    int from;
    int count;
    bool done;
  };

  struct Worker
  {
    std::atomic<bool> done;
    std::atomic<bool> terminated;
    std::atomic<Task*> task;

    Worker() : done(true), terminated(false), task(nullptr)
    {
    }
    Worker(Worker &&assign)
    {
      done.store(assign.done.load());
      terminated.store(assign.terminated.load());
      task.store(assign.task.load());
    }
    Worker(const Worker &assign)
    {
      done.store(assign.done.load());
      terminated.store(assign.terminated.load());
      task.store(assign.task.load());
    }
  };

  eastl::vector<Worker> workers;
  eastl::vector<std::thread> workersThread;

  eastl::vector<Task> tasks;

  static void worker_routine(JobManager *jobs, int worker_id)
  {
    Worker &worker = jobs->workers[worker_id];

    while (!worker.terminated.load())
    {
      if (Task *task = worker.task.load())
      {
        if (!task->done)
        {
          task->task(task->from, task->count);
          task->done = true;
          worker.done.store(true);
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  }

  JobManager()
  {
    workersCount = std::thread::hardware_concurrency();
    workers.resize(workersCount);
    workersThread.resize(workersCount);
    for (int i = 0; i < workersCount; ++i)
      workersThread[i] = eastl::move(std::thread(worker_routine, this, i));
  }

  ~JobManager()
  {
    for (int i = 0; i < workersCount; ++i)
    {
      workers[i].terminated.store(true);
      if (workersThread[i].joinable())
        workersThread[i].join();
    }
  }

  void addTask(const jobmanager::task_t &task, int from, int count)
  {
    Task &t = tasks.push_back();
    t.task = task;
    t.from = from;
    t.count = count;
    t.done = false;
  }

  void doAndWaitAllTasksDone()
  {
    int doneCount = 0;
    int currentTask = 0;
    int totalCount = tasks.size();
    while (doneCount < totalCount)
    {
      for (int i = 0; i < workersCount; ++i)
      {
        if (workers[i].done.load() && workers[i].task.load() != nullptr)
        {
          workers[i].task.store(nullptr);
          ++doneCount;
        }
        if (workers[i].done.load() && workers[i].task.load() == nullptr && currentTask < totalCount)
        {
          workers[i].done.store(false);
          workers[i].task.store(&tasks[currentTask]);
          ++currentTask;
        }
      }
    }
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

  int itemsLeft = items_count;

  int tasksCount = (items_count / chunk_size) + ((items_count % chunk_size) ? 1 : 0);
  for (int i = 0; i < items_count; i += chunk_size, itemsLeft -= chunk_size)
    jobs->addTask(task, i, eastl::min(chunk_size, itemsLeft));
}

void jobmanager::do_and_wait_all_tasks_done()
{
  ASSERT(jobs != nullptr);
  jobs->doAndWaitAllTasksDone();
}