#pragma once

#include <EASTL/functional.h>

namespace jobmanager
{
  using task_t = eastl::function<void(int /* from */, int /* cout */)>;

  void init();
  void release();

  void add_job(int chunk_size, int items_count, const task_t &task);
  void do_and_wait_all_tasks_done();
};