#include <memory>
#include <cstdlib>

#if defined(_WIN64)
static constexpr size_t default_alignment = 8;
#else
static constexpr size_t default_alignment = 4;
#endif

#define USE_JEMALLOC 0

#if USE_JEMALLOC

#include <jemalloc/jemalloc.h>

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return je_malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return je_aligned_alloc(alignment, size);
}

void* operator new(std::size_t s)
{
  return je_malloc(s);
}

void* operator new[](std::size_t s)
{
  return je_malloc(s);
}

void operator delete(void*p)
{
  je_free(p);
}

void operator delete[](void*p)
{
  je_free(p);
}

void operator delete(void *p, size_t)
{
  je_free(p);
}

void operator delete[](void *p, size_t)
{
  je_free(p);
}

#else

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return ::_aligned_malloc(size, default_alignment);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return ::_aligned_malloc(size, alignment);
}

void* operator new(std::size_t size)
{
  return ::_aligned_malloc(size, default_alignment);
}

void* operator new[](std::size_t size)
{
  return ::_aligned_malloc(size, default_alignment);
}

void operator delete(void* p)
{
  ::_aligned_free(p);
}

void operator delete[](void* p)
{
  ::_aligned_free(p);
}

void operator delete(void* p, size_t)
{
  ::_aligned_free(p);
}

void operator delete[](void* p, size_t)
{
  ::_aligned_free(p);
}

#endif