#pragma once

#include <EASTL/string.h>

namespace stacktrace
{
  void init();
  void release();

  void print(eastl::string &stack);
  void print();

  void print_all_threads();
}