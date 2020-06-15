#pragma once

#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <clang-c/Index.h>

struct VisitorState
{
  struct Parameter
  {
    eastl::string name;
    eastl::string type;
    eastl::string pureType;
    eastl::string value;
    eastl::string templateRef;

    int queryId = -1;

    bool isRW = false;

    Parameter() = default;
  };

  struct Function
  {
    int indexId = -1;

    eastl::string name;
    eastl::string kind;
    eastl::string comment;
    eastl::string filter;

    eastl::vector<Parameter> parameters;
    eastl::vector<Parameter> have;
    eastl::vector<Parameter> notHave;
    eastl::vector<Parameter> track;
  };

  struct System : Function
  {
    bool fromQuery = false;
    bool inJobs = false;
    bool addJobs = false;
    bool isBarrier = false;
    eastl::string chunkSize;
    eastl::vector<eastl::string> before;
    eastl::vector<eastl::string> after;

    eastl::string beforeStr;
    eastl::string afterStr;

    bool isEmpty() const
    {
      return parameters.size() == 1 && filter.empty() && have.empty() && notHave.empty() && track.empty();
    }
  };

  struct Query : Function
  {
    bool empty = false;
    bool lazy = false;
    CXCursor cursor;
    eastl::string components;
  };

  struct Index : Query
  {
    eastl::string componentName;
    eastl::string lookup;
  };

  struct Component
  {
    eastl::string name;
    eastl::string type;
  };

  struct Event
  {
    eastl::string name;
    eastl::string type;
  };

  struct AutoBind
  {
    struct Flag
    {
      eastl::string key;
      eastl::string value;
    };

    struct Field
    {
      eastl::string name;
      eastl::string bindName;
    };

    struct Method
    {
      bool isBuiltin = false;
      eastl::string name;
      eastl::string fullName;
      eastl::string sideEffect;
      eastl::string simNode = "das::SimNode_ExtFuncCall";
    };

    bool canNew = false;
    bool canCopy = false;

    eastl::string module;
    eastl::string type;
    eastl::vector<Flag> flags;
    eastl::vector<Field> fields;
    eastl::vector<Method> methods;
  };

  eastl::string filename;

  CXTranslationUnit unit;
  CXCursor unitCursor;

  eastl::vector<System> systems;
  eastl::vector<Query> queries;
  eastl::vector<Index> indices;
  eastl::vector<Component> components;
  eastl::vector<Event> events;
  eastl::vector<AutoBind> autoBind;
};

void parse(const CXTranslationUnit &unit, VisitorState &state);