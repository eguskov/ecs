#include <memory>

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