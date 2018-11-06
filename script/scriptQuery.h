#pragma once

#include <io/json.h>

#include <EASTL/vector.h>

class asITypeInfo;
class asIScriptObject;
struct EntityId;
struct Query;
struct CompDesc;

namespace script
{

  struct ScriptQuery
  {
    struct Iterator
    {
      ScriptQuery *sq = nullptr;
      int chunkIdx = 0;
      int posInChunk = 0;

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

  asIScriptObject* inject_components_into_struct(const EntityId &eid, const eastl::vector<CompDesc> &components, asITypeInfo *type);

}