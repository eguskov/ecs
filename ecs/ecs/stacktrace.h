#pragma once

#include <EASTL/string.h>

namespace stacktrace
{
#ifdef _DEBUG
  void init();
  void release();

  void print(eastl::string &stack);
  void print();

  void print_all_threads();
#else
  inline void init() {}
  inline void release() {}

  inline void print(eastl::string &) {}
  inline void print() {}

  inline void print_all_threads() {}
#endif
}