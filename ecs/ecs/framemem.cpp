#include "framemem.h"

static constexpr size_t default_alignment = 4;

static eastl::array<uint8_t, 4 << 20> buffer; // 4MB
static size_t buffer_offset = 0;
static size_t buffer_max_offset = 0;

uint8_t *alloc_frame_mem(size_t sz)
{ 
  return alloc_frame_mem(sz, default_alignment);
}

uint8_t *alloc_frame_mem(size_t sz, size_t alignment)
{
  uint8_t *mem = &buffer[buffer_offset];
  uint8_t *res = (uint8_t*)(((uintptr_t)mem + (alignment - 1)) & ~(alignment - 1));

  buffer_offset += sz + ((uintptr_t)res - (uintptr_t)mem);

  return res;
}

void clear_frame_mem()
{
  if (buffer_offset > buffer_max_offset)
    buffer_max_offset = buffer_offset;

  buffer_offset = 0;

#ifdef _DEBUG
  buffer.fill(0xBA);
#endif
}

size_t get_frame_mem_allocated_size()
{
  return buffer_offset;
}

size_t get_frame_mem_allocated_max_size()
{
  return buffer_max_offset;
}