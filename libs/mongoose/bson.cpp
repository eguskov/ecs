#include "bson.h"

#include <assert.h>

#include <EASTL/tuple.h>

BsonStream::BsonStream() : done(false)
{
  begin();
}

eastl::tuple<const uint8_t*, size_t> BsonStream::closeAndGetData()
{
  assert((done && documentStack.empty()) || (!done && documentStack.size() == 1));
  if (!done)
  {
    end();
    done = true;
  }
  return eastl::make_tuple(stream.data(), stream.size());
}

void BsonStream::begin()
{
  assert(!done);

  documentStack.push_back(stream.size());

  uint32_t size = 0;
  append_to_stream(stream, 4, (const uint8_t*)&size);
}

void BsonStream::begin(const char* name, uint8_t type)
{
  assert(!done);

  stream.push_back(type);
  BsonType<const char*>::add(stream, name);

  begin();
}

void BsonStream::begin(const char* name)
{
  begin(name, embedded_document_type);
}

void BsonStream::beginArray(const char* name)
{
  begin(name, array_type);
}

void BsonStream::end()
{
  assert(!done);

  stream.push_back(0);

  assert(!documentStack.empty());
  uint32_t document = documentStack.back();
  documentStack.pop_back();

  *(uint32_t*)&stream[document] = stream.size() - document;
}

void BsonStream::addName(const char* name, uint8_t type)
{
  stream.push_back(type);
  BsonType<const char*>::add(stream, name);
}

void BsonStream::add(const char* name, const char* value)
{
  addName(name, BsonType<const char*>::type);

  const int len = (int) ::strlen(value) + 1;
  BsonType<int>::add(stream, len);
  BsonType<const char*>::add(stream, value);
}

void BsonStream::add(const char* name, const glm::vec2 &value)
{
  begin(name, embedded_document_type);
  add("x", value.x);
  add("y", value.y);
  end();
}

void BsonStream::add(const char* name, const glm::vec3 &value)
{
  begin(name, embedded_document_type);
  add("x", value.x);
  add("y", value.y);
  add("z", value.z);
  end();
}

void BsonStream::add(const char* name, const glm::vec4 &value)
{
  begin(name, embedded_document_type);
  add("x", value.x);
  add("y", value.y);
  add("z", value.z);
  add("w", value.w);
  end();
}