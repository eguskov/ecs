#pragma once

#include <chrono>
#include <thread>
#include <iostream>

#include <EASTL/string.h>

struct ScopeTime
{
  using time_point = std::chrono::high_resolution_clock::time_point;

  eastl::string name;

  time_point start;

  ScopeTime(const char *_name) : name(_name)
  {
    start = std::chrono::high_resolution_clock::now();
  }

  ~ScopeTime()
  {
    std::chrono::duration<double, std::milli> diff = std::chrono::high_resolution_clock::now() - start;

    const double _d = diff.count();

    // TODO: Remove this
    std::cout << "[" << name.c_str() << "]: " << _d << " ms (" << _d * 1e3 << " us" << ")" << std::endl;
  }
};

#define PERF_TIME(name) ScopeTime __timer__##name(#name);