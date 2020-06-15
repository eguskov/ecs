#pragma once

#include "stdafx.h"

uint8_t *alloc_frame_mem(size_t sz);
uint8_t *alloc_frame_mem(size_t sz, size_t alignment);
void clear_frame_mem();

size_t get_frame_mem_allocated_size();
size_t get_frame_mem_allocated_max_size();

struct FrameMemAllocator
{
  explicit FrameMemAllocator(const char * = nullptr) {}
  FrameMemAllocator(const FrameMemAllocator &x) {}
  FrameMemAllocator(const FrameMemAllocator &x, const char *) {}

  FrameMemAllocator &operator=(const FrameMemAllocator &x) { return *this; }

  void *allocate(size_t n, int flags = 0) { return alloc_frame_mem(n); }
  void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) { return alloc_frame_mem(n, alignment); }
  void deallocate(void *p, size_t n) {}

  const char *get_name() const { return "FrameMem"; }
  void set_name(const char *) {}
};

template <typename T>
struct RawFrameMemAllocator
{
  static T *alloc()
  {
    return (T *)alloc_frame_mem(sizeof(T));
  }
};