#pragma once

#include <EASTL/hash_map.h>
#include <EASTL/string.h>

#include "io/json.h"
#include "framemem.h"

template <typename Allocator>
struct MapValue
{
  enum class Type
  {
    kUnknown,
    kInt,
    kFloat,
    kString
  };

  Allocator allocator;

  Type type = Type::kUnknown;

  union {
    int ival;
    float fval;
    char *sval;
  };

  MapValue() = default;
  MapValue(const MapValue &) = default;
  MapValue(MapValue &&) = default;

  MapValue &operator=(const MapValue &) = delete;
  MapValue &operator=(MapValue &&) = delete;

  ~MapValue()
  {
    if (type == Type::kString && sval)
      allocator.deallocate(sval, ::strlen(sval) + 1);
  }

  template <typename T>
  MapValue &operator=(const T &value);

  template <>
  MapValue &operator=(const int &value)
  {
    type = Type::kInt;
    ival = value;
    return *this;
  }

  template <>
  MapValue &operator=(const float &value)
  {
    type = Type::kFloat;
    fval = value;
    return *this;
  }

  template <>
  MapValue &operator=(const char *const &value)
  {
    type = Type::kString;
    size_t len = ::strlen(value);
    sval = (char *)allocator.allocate(len + 1);
    ::memcpy(sval, value, len);
    sval[len] = '\0';
    return *this;
  }

  template <size_t N>
  MapValue &operator=(const char (&value)[N])
  {
    type = Type::kString;
    size_t len = ::strlen(value);
    sval = (char *)allocator.allocate(len + 1);
    ::memcpy(sval, value, len);
    sval[len] = '\0';
    return *this;
  }
};

using FrameMemString = eastl::basic_string<char, FrameMemAllocator>;

namespace eastl
{

template <>
struct hash<FrameMemString>
{
  size_t operator()(const FrameMemString &x) const
  {
    const unsigned char *p = (const unsigned char *)x.c_str(); // To consider: limit p to at most 256 chars.
    unsigned int c, result = 2166136261U;                      // We implement an FNV-like string hash.
    while ((c = *p++) != 0)                                    // Using '!=' disables compiler warnings.
      result = (result * 16777619) ^ c;
    return (size_t)result;
  }
};

} // namespace eastl

using Map = eastl::hash_map<eastl::string, MapValue<EASTLAllocatorType>>;

template <typename T>
using FrameMemHashMap = eastl::hash_map<FrameMemString, T, eastl::hash<FrameMemString>, eastl::equal_to<FrameMemString>, FrameMemAllocator>;

using FrameMemMap = FrameMemHashMap<MapValue<FrameMemAllocator>>;

Map json_to_map(const JDocument &doc);