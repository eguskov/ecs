#include <memory>

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return new char[size];
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
  return new char[size];
}