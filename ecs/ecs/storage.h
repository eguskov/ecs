#pragma once

#include "stdafx.h"

#ifdef _DEBUG
#define sassert(...) assert(__VA_ARGS__)
#else
#define sassert(...)
#endif

// TODO: Remove allocation by free. With archtype it's useless here.
struct Storage
{
  int freeCount = 0;
  int totalCount = 0;
  int elemSize = 0;
  int lastFreeIndex = -1;
  int nextAllocIndex = 0;

  uint8_t* dataCached = nullptr;
  int sizeCached = 0;

  eastl::bitvector<> freeMask;

  virtual ~Storage() {}

  void invalidate()
  {
    dataCached = data();
    sizeCached = size();
  }

  virtual eastl::tuple<uint8_t*, int> allocate() = 0;
  virtual void deallocate(int offset) = 0;

  virtual uint8_t* data() = 0;
  virtual int size() = 0;

  virtual void copyTo(Storage *dst) = 0;

  uint8_t* getRaw(int offset)
  {
    sassert(dataCached == data());
    sassert((offset % elemSize) == 0);
    sassert(freeMask[offset / elemSize] == false);
    return &dataCached[offset];
  }

  template <typename T>
  const T& get(int offset) const
  {
    sassert(dataCached == data());
    sassert((offset % elemSize) == 0);
    sassert(freeMask[offset / elemSize] == false);
    return *(T*)&dataCached[offset];
  }

  template <typename T>
  T& get(int offset)
  {
    sassert(dataCached == data());
    sassert((offset % elemSize) == 0);
    sassert(freeMask[offset / elemSize] == false);
    return *(T*)&dataCached[offset];
  }

  uint8_t* getRawByIndex(int idx)
  {
    sassert(dataCached == data());
    return dataCached + (idx * elemSize);
  }

  template <typename T>
  const T& getByIndex(int idx) const
  {
    sassert(dataCached == data());
    sassert(freeMask[idx] == false);
    return *(T*)&dataCached[idx * elemSize];
  }

  template <typename T>
  T& getByIndex(int idx)
  {
    sassert(dataCached == data());
    sassert(freeMask[idx] == false);
    return *(T*)&dataCached[idx * elemSize];
  }
};

template <typename T>
struct StorageSpec final : Storage
{
  eastl::vector<T> items;

  virtual ~StorageSpec()
  {
    int i = 0;
    for (auto &item : items)
      if (!freeMask[i++])
        item.~T();
    
    if (items.data())
    {
      auto &a = items.get_allocator();
      a.deallocate(items.data(), items.size() * sizeof(T));
    }
    items.reset_lose_memory();
  }

  uint8_t* data() override final
  {
    return (uint8_t*)items.data();
  }

  int size() override final
  {
    return items.size() * elemSize;
  }

  void copyTo(Storage *dst)
  {
    (*static_cast<StorageSpec<T>*>(dst)) = *this;
  }

  eastl::tuple<uint8_t*, int> allocate() override final
  {
    if (lastFreeIndex >= 0)
    {
      sassert(freeCount > 0);
      --freeCount;
      const int i = lastFreeIndex;
      lastFreeIndex = -1;
      freeMask[i] = false;
      uint8_t *mem = (uint8_t*)&items[i];
      new (mem) T;
      return eastl::make_tuple(mem, i * elemSize);
    }
    else if (freeCount > 0)
    {
      for (int i = 0; i < (int)freeMask.size(); ++i)
        if (freeMask[i])
        {
          --freeCount;
          freeMask[i] = false;
          uint8_t *mem = (uint8_t*)&items[i];
          new (mem) T;
          return eastl::make_tuple(mem, i * elemSize);
        }

      sassert(false);
    }

    const int i = nextAllocIndex;
    if (nextAllocIndex++ >= totalCount)
    {
      ++totalCount;
      items.resize(totalCount);
      freeMask.resize(totalCount);

      invalidate();
    }

    uint8_t *mem = (uint8_t*)&items[i];
    new (mem) T;

    freeMask[i] = false;

    return eastl::make_tuple((uint8_t*)&items[i], i * elemSize);
  }

  void deallocate(int offset) override final
  {
    const int i = offset / elemSize;

    sassert((offset % elemSize) == 0);
    sassert(i >= 0 && i < totalCount);
    sassert(freeMask[i] == false);

    if (i == totalCount - 1)
      nextAllocIndex = i;
    else
    {
      ++freeCount;
      lastFreeIndex = i;
    }

    freeMask[i] = true;
    items[i].~T();
  }
};