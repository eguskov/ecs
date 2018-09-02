#pragma once

#include <stdint.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/matrix.hpp>

static inline void append_to_stream(eastl::vector<uint8_t> &stream, int sz, const uint8_t *data)
{
  stream.resize(stream.size() + sz);
  ::memcpy_s(&stream[stream.size() - sz], sz, data, sz);
}

template <typename T>
struct BsonType
{
};

template <> struct BsonType<int>
{
  static const uint8_t type = 0x10;
  static void add(eastl::vector<uint8_t> &stream, const int &value) { append_to_stream(stream, 4, (const uint8_t*)&value); }
};
template <> struct BsonType<uint32_t>
{
  static const uint8_t type = 0x10;
  static void add(eastl::vector<uint8_t> &stream, const uint32_t &value) { append_to_stream(stream, 4, (const uint8_t*)&value); }
};
template <> struct BsonType<uint16_t>
{
  static const uint8_t type = 0x10;
  static void add(eastl::vector<uint8_t> &stream, const uint16_t &value) { uint32_t v = value; BsonType<uint32_t>::add(stream, v); }
};
template <> struct BsonType<uint8_t>
{
  static const uint8_t type = 0x10;
  static void add(eastl::vector<uint8_t> &stream, const uint8_t &value) { uint32_t v = value; BsonType<uint32_t>::add(stream, v); }
};
template <> struct BsonType<double>
{
  static const uint8_t type = 0x01;
  static void add(eastl::vector<uint8_t> &stream, const double &value) { append_to_stream(stream, 8, (const uint8_t*)&value); }
};
template <> struct BsonType<float>
{
  static const uint8_t type = 0x01;
  static void add(eastl::vector<uint8_t> &stream, const float &value) { double v = value; BsonType<double>::add(stream, v); }
};
template <> struct BsonType<bool>
{
  static const uint8_t type = 0x08;
  static void add(eastl::vector<uint8_t> &stream, const bool &value) { uint8_t v = value ? 1 : 0; append_to_stream(stream, 1, (const uint8_t*)&v); }
};
template <> struct BsonType<const char*>
{
  static const uint8_t type = 0x02;
  static void add(eastl::vector<uint8_t> &stream, const char* const &value) { append_to_stream(stream, ::strlen(value) + 1, (const uint8_t*)value); }
};

class BsonStream
{
  static const uint8_t embedded_document_type = 0x03;
  static const uint8_t array_type = 0x04;

  bool done;
  eastl::vector<uint8_t> stream;
  eastl::vector<uint32_t> documentStack;

  void begin();
  void begin(const char* name, uint8_t type);

  void addName(const char* name, uint8_t type);

  template <typename T>
  void add(const char* name, const T &value, uint8_t type)
  {
    assert(!done);

    addName(name, type);
    BsonType<T>::add(stream, value);
  }

public:
  struct StringIndex
  {
    int num;
    int intIndex;
    eastl::array<char, 16> index;

    StringIndex() : num(1), intIndex(0)
    {
      index.fill(0);
      index[0] = '0';
    }

    const char* str() const
    {
      return &index[0];
    }

    int idx() const
    {
      return intIndex;
    }

    void increment()
    {
      ++intIndex;

      ++index[num - 1];
      if (index[num - 1] <= 0x39)
        return;

      for (int j = num - 1; j >= 0; --j)
      {
        if (index[j] <= 0x39)
          break;

        if (j == 0)
        {
          index[num] = '0';
          index[0] = '1';
          ++num;
        }
        else
        {
          index[j] = '0';
          ++index[j - 1];
        }
      }
    }
  };

  BsonStream();

  eastl::tuple<const uint8_t*, size_t> closeAndGetData();

  void begin(const char* name);
  void beginArray(const char* name);
  void end();

  bool isOpened() const { return !done; };

  template <typename T>
  void add(const char* name, const T &value)
  {
    add(name, value, BsonType<T>::type);
  }

  template <typename T>
  void add(const char* name, const eastl::vector<T> &value)
  {
    assert(!done);

    begin(name, array_type);
    for (StringIndex index; index.idx() < value.count(); index.increment())
      add(index.str(), value[index.idx()]);
    end();
  }

  template <typename T, typename Writer>
  void add(const char* name, const eastl::vector<T> &value, Writer& writer)
  {
    assert(!done);

    begin(name, array_type);
    for (StringIndex index; index.idx() < value.count(); index.increment())
    {
      begin(index.str());
      writer.writeToBson(*this, value[index.idx()]);
      end();
    }
    end();
  }

  template <typename T, typename Writer>
  void add(const char* name, const T &value, Writer& writer)
  {
    assert(!done);
    begin(name);
    writer.writeToBson(*this, value);
    end();
  }

  void add(const char* name, const char* value);
  void add(const char* name, char* value) { add(name, (const char*)value); }
  void add(const char* name, const eastl::string &value) { add(name, value.c_str()); }
  void add(const char* name, const glm::vec2 &value);
  void add(const char* name, const glm::vec3 &value);
  void add(const char* name, const glm::vec4 &value);
};
