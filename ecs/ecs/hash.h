#pragma once

#include <EASTL/hash_map.h>

namespace hash
{
  template <typename S> struct fnv_internal;
  template <typename S> struct fnv1;
  template <typename S> struct fnv1a;

  template <> struct fnv_internal<uint32_t>
  {
    constexpr static uint32_t default_offset_basis = 0x811C9DC5UL;
    constexpr static uint32_t prime                = 0x01000193UL;
  };

  template <> struct fnv1<uint32_t> : public fnv_internal<uint32_t>
  {
    constexpr static inline uint32_t hash(char const*const aString, const uint32_t val = default_offset_basis)
    {
      return (aString[0] == '\0') ? val : hash(&aString[1], uint32_t(uint64_t(val) * uint64_t(prime)) ^ uint32_t(aString[0]));
    }
  };

  template <> struct fnv1a<uint32_t> : public fnv_internal<uint32_t>
  {
    constexpr static inline uint32_t hash(char const*const aString, const uint32_t val = default_offset_basis)
    {
      return (aString[0] == '\0') ? val : hash(&aString[1], uint32_t(uint64_t(val ^ uint32_t(aString[0])) * uint64_t(prime)));
    }
  };
}

struct ConstHashedString
{
  uint32_t hash = 0;
  char const* const str;

  constexpr ConstHashedString(char const* const s) : str(s), hash(hash::fnv1a<uint32_t>::hash(s)) {}

  bool operator==(const ConstHashedString &rhs) const { return hash == rhs.hash; }
  bool operator<(const ConstHashedString &rhs) const { return hash < rhs.hash; }
  bool operator>(const ConstHashedString &rhs) const { return hash > rhs.hash; }
};

namespace hash
{
  inline uint32_t str(const char *s) { return fnv1a<uint32_t>::hash(s); }
  inline constexpr ConstHashedString cstr(char const* const s) { return ConstHashedString(s); }
}

struct HashedString
{
  bool isOwnMemory = false;
  uint32_t hash = 0;
  const char *str = nullptr;

  HashedString() = default;
  ~HashedString()
  {
    if (isOwnMemory) ::free((void*)str);
  }
  HashedString(const char *s) : str(::_strdup(s)), hash(hash::str(s)), isOwnMemory(true) {}
  HashedString(const ConstHashedString &s) : str(s.str), hash(s.hash), isOwnMemory(false) {}
  HashedString(const HashedString &s)
  {
    *this = s;
  }

  HashedString& operator=(const HashedString &s)
  {
    if (this == &s)
      return *this;
    if (isOwnMemory && str)
      ::free((void*)str);
    hash = s.hash;
    isOwnMemory = s.isOwnMemory;
    if (isOwnMemory)
      str = ::_strdup(s.str);
    else
      str = s.str;

    return *this;
  }

  operator bool() const { return hash != 0 && str != nullptr; }

  bool operator==(const HashedString &rhs) const { return hash == rhs.hash; }
  bool operator<(const HashedString &rhs) const { return hash < rhs.hash; }
  bool operator>(const HashedString &rhs) const { return hash > rhs.hash; }
};

inline HashedString hash_str(char const* const s) { return HashedString(s); }

namespace eastl
{
  template <> struct hash<HashedString>
  {
    size_t operator()(const HashedString &val) const { return val.hash; }
  };

  template <> struct hash<ConstHashedString>
  {
    size_t operator()(const ConstHashedString &val) const { return val.hash; }
  };
}