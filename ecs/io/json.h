#pragma once

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "ecs/framemem.h"

using JDocument = rapidjson::Document;
using JValue = rapidjson::Value;

struct JFrameAllocator
{
  static const bool kNeedFree = false;

  void* Malloc(size_t size)
  {
    if (size)
      return alloc_frame_mem(size);
    else
      return nullptr;
  }

  void* Realloc(void *orig_ptr, size_t orig_sz, size_t new_sz)
  {
    void *mem = alloc_frame_mem(new_sz);
    if (orig_ptr)
      ::memmove(mem, orig_ptr, orig_sz);
    return mem;
  }

  static void Free(void *) {}
};

using JFrameDocument = rapidjson::GenericDocument<rapidjson::UTF8<>, JFrameAllocator, JFrameAllocator>;
using JFrameValue = rapidjson::GenericValue<rapidjson::UTF8<>, JFrameAllocator>;