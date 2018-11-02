#pragma once

namespace hash
{
  template <typename S> struct fnv_internal;
  template <typename S> struct fnv1;
  template <typename S> struct fnv1a;

  template <> struct fnv_internal<uint32_t>
  {
    constexpr static uint32_t default_offset_basis = 0x811C9DC5;
    constexpr static uint32_t prime                = 0x01000193;
  };

  template <> struct fnv1<uint32_t> : public fnv_internal<uint32_t>
  {
    constexpr static inline uint32_t hash(char const*const aString, const uint32_t val = default_offset_basis)
    {
      return (aString[0] == '\0') ? val : hash( &aString[1], ( val * prime ) ^ uint32_t(aString[0]) );
    }
  };

  template <> struct fnv1a<uint32_t> : public fnv_internal<uint32_t>
  {
    constexpr static inline uint32_t hash(char const*const aString, const uint32_t val = default_offset_basis)
    {
      return (aString[0] == '\0') ? val : hash( &aString[1], ( val ^ uint32_t(aString[0]) ) * prime);
    }
  };

  struct ConstHashedString
  {
    uint32_t hash = 0;
    char const* const str;

    constexpr ConstHashedString(char const* const s) : str(s), hash(fnv1<uint32_t>::hash(s)) {}
  };


  constexpr ConstHashedString cstr(char const* const s) { return ConstHashedString(s); }
  inline uint32_t str(const char *s) { return fnv1<uint32_t>::hash(s); }
}

struct HashedString
{
  uint32_t hash = 0;
  // TODO: Replace with eastl::string. Because of script!
  std::string str;
  // eastl::string str;

  HashedString() = default;
  HashedString(const char *s) : str(s), hash(hash::str(s)) {}
};