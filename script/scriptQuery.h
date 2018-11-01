#pragma once

class asITypeInfo;
struct Query;

namespace script
{

  struct ScriptQuery
  {
    struct Iterator
    {
      ScriptQuery *sq = nullptr;
      int pos = 0;

      Iterator& operator=(const Iterator &assign);
      void operator++();
      bool hasNext() const;
      void* get();
    };

    asITypeInfo *type = nullptr;
    Query *query = nullptr;

    ~ScriptQuery() {}

    Iterator perform();
  };

}