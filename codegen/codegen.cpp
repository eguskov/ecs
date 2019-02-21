#include "stdafx.h"

#include <clang-c\Index.h>

#include <Windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cassert>

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/functional.h>

#include "queryparser.h"

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
}

namespace utils
{
  static bool startsWith(const eastl::string &str, const eastl::string &prefix)
  {
    return str.size() >= prefix.size() &&
      str.compare(0, prefix.size(), prefix) == 0;
  }

  static bool endsWith(const eastl::string &str, const eastl::string &suffix)
  {
    return str.size() >= suffix.size() &&
      str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  template<typename T>
  static eastl::string join(const eastl::vector<T> &v, const eastl::string &token) {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = v.begin(); i != v.end(); i++) {
      if (i != v.begin())
        result << token;
      result << *i;
    }

    return eastl::string(result.str().c_str());
  }

  template<typename T, typename C>
  static eastl::string join(const eastl::vector<T> &v, C func, const eastl::string &token)
  {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = v.begin(); i != v.end(); i++)
    {
      auto s = func(i);
      if (i != v.begin())
        result << token;
      result << s;
    }

    return eastl::string(result.str().c_str());
  }

  template<typename T, typename C>
  static eastl::string join(
    const typename eastl::vector<T>::const_iterator &from,
    const typename eastl::vector<T>::const_iterator &to,
    C func,
    const eastl::string &token)
  {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = from; i != to; i++)
    {
      auto s = func(i);
      if (i != from)
        result << token;
      result << s;
    }

    return eastl::string(result.str().c_str());
  }

  static eastl::vector<eastl::string> split(const eastl::string &str, char delim)
  {
    eastl::vector<eastl::string> elems;
    std::stringstream ss(str.c_str());

    std::string item;
    while (std::getline(ss, item, delim))
      elems.emplace_back(item.c_str());

    return elems;
  }
}

struct ScopeString
{
  ScopeString(CXString&& s) : s(eastl::move(s)) {};
  ~ScopeString() { clang_disposeString(s); };

  ScopeString(const ScopeString&) = delete;
  ScopeString& operator=(const ScopeString&) = delete;
  ScopeString(ScopeString&&) = delete;
  ScopeString& operator=(ScopeString&&) = delete;

  operator const char*() const { return str(); }
  const char* str() const { return clang_getCString(s); }

  CXString s;
};

static inline eastl::string to_string(const ScopeString &s)
{
  return eastl::string(s.str());
}

static inline void to_string(const ScopeString &s, eastl::string &out_str)
{
  out_str.assign(s.str());
}

static inline eastl::string to_string(CXString &&s)
{
  ScopeString str(eastl::move(s));
  return eastl::string(str.str());
}

static inline void to_string(CXString &&s, eastl::string &out_str)
{
  ScopeString str(eastl::move(s));
  out_str.assign(str.str());
}

static std::ostream& operator<<(std::ostream &out, const eastl::string &s)
{
  out << s.c_str();
  return out;
}

struct DumpState
{
  int level = 0;
};

CXChildVisitResult dump_cursor_visitor(CXCursor cursor, CXCursor parent, CXClientData data)
{
  //if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
  //  return CXChildVisit_Continue;

  int level = *(int*)data;
  int nextLevel = level + 1;

  std::cout <<
    eastl::string(level, '-') << " " <<
    to_string(clang_getCursorKindSpelling(clang_getCursorKind(cursor))) <<
    " (" << to_string(clang_getCursorSpelling(cursor)) << ")" <<
    std::endl;

  clang_visitChildren(cursor, dump_cursor_visitor, &nextLevel);

  return CXChildVisit_Continue;
}

void dump_cursor(CXCursor cursor, CXCursor parent)
{
  int level = 0;
  dump_cursor_visitor(cursor, parent, &level);
}

void dump_cursor(CXCursor cursor)
{
  int level = 0;
  dump_cursor_visitor(cursor, clang_getNullCursor(), &level);
}

eastl::string get_type_ref(CXCursor cursor)
{
  eastl::string ref;
  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    auto &ref = *static_cast<eastl::string*>(data);
    if (clang_getCursorKind(cursor) == CXCursor_TypeRef)
    {
      ref = to_string(clang_getCursorSpelling(cursor));
      return CXChildVisit_Break;
    }
    return CXChildVisit_Recurse;
  };
  clang_visitChildren(cursor, visitor, &ref);

  ref = std::regex_replace(ref.c_str(), std::regex("struct "), "").c_str();

  return ref;
}

eastl::string escape_name(const eastl::string name)
{
  return std::regex_replace(name.c_str(), std::regex("::"), "_").c_str();
}

template<typename T>
void read_function_params(CXCursor cursor, T &parameters)
{
  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    T &parameters = *static_cast<T*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_ParmDecl)
    {
      auto &p = parameters.push_back();
      p.name = to_string(clang_getCursorSpelling(cursor));
      p.type = to_string(clang_getTypeSpelling(clang_getCursorType(cursor)));
      p.pureType = std::regex_replace(p.type.c_str(), std::regex("(?:const|\\&|(?:\\s+))"), "").c_str();
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &parameters);
}

CXCursor find_method(CXCursor cursor, const char *name)
{
  struct State
  {
    const char *name;
    CXCursor res;
  } state;

  state.name = name;
  state.res = clang_getNullCursor();

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    State &state = *static_cast<State*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_CXXMethod && to_string(clang_getCursorSpelling(cursor)) == state.name)
    {
      state.res = cursor;
      return CXChildVisit_Break;
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &state);

  return state.res;
}

eastl::string read_string_literal(CXCursor cursor)
{
  eastl::string res;

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    eastl::string &res = *static_cast<eastl::string*>(data);
    if (clang_getCursorKind(cursor) == CXCursor_StringLiteral)
    {
      res = to_string(clang_getCursorSpelling(cursor));
      if (res.length() > 0 && res.cbegin()[0] == '"')
        res.erase(0, 1);
      if (res.length() > 0 && res.crbegin()[0] == '"')
        res.erase(res.length() - 1, 1);
      return CXChildVisit_Break;
    }
    return CXChildVisit_Recurse;
  };

  clang_visitChildren(cursor, visitor, &res);

  return res;
}

template<typename T>
void read_struct_fields(CXCursor cursor, T &fields)
{
  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    T &fields = *static_cast<T*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_FieldDecl || kind == CXCursor_VarDecl)
    {
      auto &p = fields.push_back();
      p.name = to_string(clang_getCursorSpelling(cursor));
      p.type = to_string(clang_getTypeSpelling(clang_getCursorType(cursor)));
      p.pureType = std::regex_replace(p.type.c_str(), std::regex("(?:const|\\&|(?:\\s+))"), "").c_str();

      if (kind == CXCursor_VarDecl)
        p.value = read_string_literal(cursor);
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &fields);
}

template<typename C>
void foreach_struct_decl(CXCursor cursor, const C &cb)
{
  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    const C &cb = *static_cast<const C*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_StructDecl)
    {
      auto structName = to_string(clang_getCursorSpelling(cursor));
      cb(cursor, structName);
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, (void*)&cb);
}

struct VisitorState
{
  struct Parameter
  {
    eastl::string name;
    eastl::string type;
    eastl::string pureType;
    eastl::string value;

    int queryId = -1;
  };

  struct Function
  {
    int version = 1;

    eastl::string name;
    eastl::string kind;
    eastl::string comment;
    eastl::string filter;

    eastl::vector<Parameter> parameters;
    eastl::vector<Parameter> have;
    eastl::vector<Parameter> notHave;
    eastl::vector<Parameter> trackTrue;
    eastl::vector<Parameter> trackFalse;
  };

  struct System : Function
  {
    bool fromQuery = false;
  };

  struct Query : Function
  {
    bool empty = false;
    CXCursor cursor;
    eastl::string components;
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

  eastl::string filename;

  CXTranslationUnit unit;
  CXCursor unitCursor;

  eastl::vector<System> systems;
  eastl::vector<Query> queries;
  eastl::vector<Component> components;
  eastl::vector<Event> events;
};

int main(int argc, char* argv[])
{
  std::cout << "Codegen: " << argv[1] << std::endl;

  char path[MAX_PATH];
  ::GetCurrentDirectory(MAX_PATH, path);

  // TODO: Remove path hardcode or rewrite codegen on AngelScript
  const char *clangArgs[] = {
    "-xc++",
    "-std=c++14",
    "-w",
    "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
    "-D__CODEGEN__",
    "-Wno-invalid-token-paste",
    "-ferror-limit=0",
    "-fsyntax-only",
    "-fdelayed-template-parsing",
    "-fdiagnostics-show-template-tree",
    "-ftemplate-depth=900",
    "-fms-compatibility",
    "-fms-extensions",
    "-fmsc-version=1900",
    "-I../ecs",
    "-I../script",
    "-I../libs/rapidjson/include",
    "-I../libs/glm",
    "-I../libs/mongoose",
    "-I../libs/angelscript/angelscript/include",
    "-I../libs/angelscript/add_on",
    "-I../libs/EASTL/include",
    "-I../libs/EASTL/test/packages/EABase/include/Common",
    "-I../libs/raylib/src",
    "-I../libs/Box2D",
    "-IC:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.14.26428/include",
    "-IC:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.14.26428/atlmfc/include",
    "-IC:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Auxiliary/VS/include",
    "-IC:/Program Files (x86)/Windows Kits/10/Include/10.0.17134.0/ucrt",
    "-IC:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Auxiliary/VS/UnitTest/include",
    "-IC:/Program Files (x86)/Windows Kits/10/Include/10.0.17134.0/um",
    "-IC:/Program Files (x86)/Windows Kits/10/Include/10.0.17134.0/shared",
    "-IC:/Program Files (x86)/Windows Kits/10/Include/10.0.17134.0/winrt",
    "-IC:/Program Files (x86)/Windows Kits/10/Include/10.0.17134.0/cppwinrt",
    "-IC:/Program Files (x86)/Windows Kits/NETFXSDK/4.6.1/Include/um"
  };
  const int clangArgsCount = _countof(clangArgs);

  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit unit = clang_parseTranslationUnit(
    index,
    argv[1],
    clangArgs, clangArgsCount,
    nullptr, 0,
    CXTranslationUnit_None | CXTranslationUnit_DetailedPreprocessingRecord);
  if (unit == nullptr)
  {
    std::cerr << "Unable to parse translation unit: " << path << "\\" << argv[1] << std::endl;
    return -1;
  }

  VisitorState state;

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    VisitorState &state = *static_cast<VisitorState*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursorKind parentKind = clang_getCursorKind(parent);

    ScopeString spelling = clang_getCursorSpelling(cursor);
    ScopeString kindSpelling = clang_getCursorKindSpelling(kind);

    if (kind == CXCursor_Namespace || kind == CXCursor_NamespaceAlias || kind == CXCursor_NamespaceRef)
      return CXChildVisit_Recurse;

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl)
    {
      auto structVisitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
      {
        VisitorState &state = *static_cast<VisitorState*>(data);

        CXCursorKind kind = clang_getCursorKind(cursor);

        if (kind == CXCursor_AnnotateAttr)
        {
          auto name = to_string(clang_getCursorSpelling(cursor));
          auto structName = to_string(clang_getCursorSpelling(parent));

          auto p = clang_getCursorSemanticParent(parent);
          while (!clang_Cursor_isNull(p) && !clang_equalCursors(p, state.unitCursor))
          {
            structName = to_string(clang_getCursorSpelling(p)) + "::" + structName;
            p = clang_getCursorSemanticParent(p);
          }

          if (utils::startsWith(name, "@component: "))
          {
            if (!clang_Location_isFromMainFile(clang_getCursorLocation(parent)))
              return CXChildVisit_Continue;

            auto &comp = state.components.push_back();
            comp.type = eastl::move(structName);
            comp.name = name.substr(::strlen("@component: "));
          }
          else if (name == "@event")
          {
            if (!clang_Location_isFromMainFile(clang_getCursorLocation(parent)))
              return CXChildVisit_Continue;

            auto &comp = state.events.push_back();
            comp.type = eastl::move(structName);
            comp.name = comp.type;
          }
          else if (name == "@query")
          {
            auto &q = state.queries.push_back();
            q.name = eastl::move(structName);
          }
          else if (utils::startsWith(name, "@have: "))
          {
            auto it = eastl::find_if(state.queries.begin(), state.queries.end(), [&](const VisitorState::Query &f) { return f.name == structName; });
            if (it != state.queries.end())
            {
              auto &p = it->have.emplace_back();
              p.name = name.substr(::strlen("@have: "));
            }
          }
          else if (utils::startsWith(name, "@not-have: "))
          {
            auto it = eastl::find_if(state.queries.begin(), state.queries.end(), [&](const VisitorState::Query &f) { return f.name == structName; });
            if (it != state.queries.end())
            {
              auto &p = it->notHave.emplace_back();
              p.name = name.substr(::strlen("@not-have: "));
            }
          }
          else if (utils::startsWith(name, "@is-true: "))
          {
            auto it = eastl::find_if(state.queries.begin(), state.queries.end(), [&](const VisitorState::Query &f) { return f.name == structName; });
            if (it != state.queries.end())
            {
              auto &p = it->trackTrue.emplace_back();
              p.name = name.substr(::strlen("@is-true: "));
            }
          }
          else if (utils::startsWith(name, "@is-false: "))
          {
            auto it = eastl::find_if(state.queries.begin(), state.queries.end(), [&](const VisitorState::Query &f) { return f.name == structName; });
            if (it != state.queries.end())
            {
              auto &p = it->trackFalse.emplace_back();
              p.name = name.substr(::strlen("@is-false: "));
            }
          }
        }

        

        return CXChildVisit_Continue;
      };

      bool isQuery = false;
      bool isSystem = false;
      foreach_struct_decl(cursor, [&isQuery](CXCursor, const eastl::string &name) { if (name == "ecs_query") isQuery = true; });
      foreach_struct_decl(cursor, [&isSystem](CXCursor, const eastl::string &name) { if (name == "ecs_system") isSystem = true; });

      if (isQuery)
      {
        auto structName = to_string(clang_getCursorSpelling(cursor));
        auto &q = state.queries.push_back();
        q.version = 2;
        q.name = eastl::move(structName);

        read_struct_fields(cursor, q.parameters);

        foreach_struct_decl(cursor, [&](CXCursor cursor, const eastl::string &name)
        {
          if (name == "ql_have")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);
            for (const auto &f : fields)
              q.have.emplace_back().name = f.name;
          }
          else if (name == "ql_not_have")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);
            for (const auto &f : fields)
              q.notHave.emplace_back().name = f.name;
          }
          else if (name == "ql_where")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);

            QueryComponents outComps;
            q.filter = parse_where_query(fields[0].value, outComps);

            for (const auto &c : outComps)
            {
              auto res = eastl::find_if(q.have.begin(), q.have.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == q.have.end())
                q.have.emplace_back().name = c.name;
            }
          }
        });
      }

      if (isSystem)
      {
        auto structName = to_string(clang_getCursorSpelling(cursor));
        auto &s = state.systems.push_back();
        s.version = 2;
        s.name = eastl::move(structName);

        CXCursor runCursor = find_method(cursor, "run");
        assert(!clang_equalCursors(runCursor, clang_getNullCursor()));

        read_function_params(runCursor, s.parameters);

        foreach_struct_decl(cursor, [&](CXCursor cursor, const eastl::string &name)
        {
          if (name == "ql_have")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);
            for (const auto &f : fields)
              s.have.emplace_back().name = f.name;
          }
          else if (name == "ql_not_have")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);
            for (const auto &f : fields)
              s.notHave.emplace_back().name = f.name;
          }
          else if (name == "ql_where")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);

            QueryComponents outComps;
            s.filter = parse_where_query(fields[0].value, outComps);

            for (const auto &c : outComps)
            {
              auto res = eastl::find_if(s.have.begin(), s.have.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == s.have.end())
                s.have.emplace_back().name = c.name;
            }
          }
          else if (name == "ql_join")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);

            s.filter = fields[0].value;
          }
        });
      }

      clang_visitChildren(cursor, structVisitor, data);

      return CXChildVisit_Continue;
    }

    if (kind == CXCursor_FunctionDecl || kind == CXCursor_FunctionTemplate)
    {
      auto functionVisitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
      {
        VisitorState &state = *static_cast<VisitorState*>(data);

        CXCursorKind kind = clang_getCursorKind(cursor);

        if (kind == CXCursor_AnnotateAttr)
        {
          auto name = to_string(clang_getCursorSpelling(cursor));
          auto funcName = to_string(clang_getCursorSpelling(parent));

          if (name == "@system")
          {
            auto &sys = state.systems.push_back();
            sys.name = eastl::move(funcName);

            read_function_params(parent, sys.parameters);
          }
          else if (utils::startsWith(name, "@have: "))
          {
            auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::System &f) { return f.name == funcName; });
            if (it != state.systems.end())
            {
              auto &p = it->have.emplace_back();
              p.name = name.substr(::strlen("@have: "));
            }
          }
          else if (utils::startsWith(name, "@not-have: "))
          {
            auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::System &f) { return f.name == funcName; });
            if (it != state.systems.end())
            {
              auto &p = it->notHave.emplace_back();
              p.name = name.substr(::strlen("@not-have: "));
            }
          }
          else if (utils::startsWith(name, "@is-true: "))
          {
            auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::System &f) { return f.name == funcName; });
            if (it != state.systems.end())
            {
              auto &p = it->trackTrue.emplace_back();
              p.name = name.substr(::strlen("@is-true: "));
            }
          }
          else if (utils::startsWith(name, "@is-false: "))
          {
            auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::System &f) { return f.name == funcName; });
            if (it != state.systems.end())
            {
              auto &p = it->trackFalse.emplace_back();
              p.name = name.substr(::strlen("@is-false: "));
            }
          }
        }
        else if (kind == CXCursor_CompoundStmt)
        {
          //eastl::string funcName = to_string(clang_getCursorSpelling(parent));
          //auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::Function &f) { return f.name == funcName; });
          //if (it != state.systems.end())
          {
            auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
            {
              VisitorState &state = *static_cast<VisitorState*>(data);

              CXCursorKind kind = clang_getCursorKind(cursor);

              if (kind == CXCursor_FirstExpr)
                return CXChildVisit_Recurse;

              if (kind == CXCursor_CallExpr)
              {
                struct
                {
                  CXClientData data = nullptr;
                  eastl::string type;
                } labmdaState;

                auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
                {
                  auto &state = *static_cast<decltype(labmdaState)*>(data);
                  auto &parentState = *static_cast<VisitorState*>(state.data);

                  CXCursorKind kind = clang_getCursorKind(cursor);

                  if (state.type.empty() && kind == CXCursor_FirstExpr)
                  {
                    state.type = get_type_ref(cursor);
                    return CXChildVisit_Continue;
                  }

                  if (kind == CXCursor_FirstExpr || kind == CXCursor_CallExpr)
                    return CXChildVisit_Recurse;

                  if (kind == CXCursor_LambdaExpr)
                  {
                    auto it = eastl::find_if(parentState.queries.begin(), parentState.queries.end(), [&](const VisitorState::Query &f) { return f.name == state.type; });
                    if (it != parentState.queries.end() && it->parameters.empty())
                      read_function_params(cursor, it->parameters);
                    return CXChildVisit_Continue;
                  }

                  return CXChildVisit_Continue;
                };

                labmdaState.data = data;
                clang_visitChildren(cursor, visitor, &labmdaState);

                return CXChildVisit_Continue;
              }

              return CXChildVisit_Continue;
            };

            clang_visitChildren(cursor, visitor, data);
          }
        }

        return CXChildVisit_Continue;
      };

      clang_visitChildren(cursor, functionVisitor, data);

      return CXChildVisit_Continue;
    }

    return CXChildVisit_Continue;
  };

  state.unit = unit;
  state.filename = argv[1];

  CXCursor cursor = clang_getTranslationUnitCursor(unit);
  state.unitCursor = cursor;
  //dump_cursor(cursor, clang_getNullCursor());
  clang_visitChildren(cursor, visitor, &state);
  CXFile unitFile = clang_getFile(unit, argv[1]);

  std::cerr << "Done:" << std::endl;
  std::cerr << "  systems: " << state.systems.size() << std::endl;
  std::cerr << "  queries: " << state.queries.size() << std::endl;
  std::cerr << "  components: " << state.components.size() << std::endl;
  std::cerr << "  events: " << state.events.size() << std::endl;

  {
    eastl::string filename = state.filename + ".gen";

    std::ofstream out;
    out.open(filename.c_str(), std::ofstream::out);
    if (!out.is_open())
    {
      std::cerr << "Cannon open: " << filename << std::endl;
      return -1;
    }

    eastl::string basename;

    std::cmatch match;
    std::regex re = std::regex("(?:.*)(?:\\\\|\\/)(.*?)\\.(h|cpp)$");
    std::regex_search(state.filename.cbegin(), state.filename.cend(), match, re);
    if (!match.empty())
      basename.assign((match[1].str() + "." + match[2].str()).c_str());
    else
      basename = state.filename;

    out << "//! GENERATED FILE\n\n" << std::endl;
    out << "#ifndef __CODEGEN__\n" << std::endl;

    out << "#include \"" << basename << "\"\n" << std::endl;

    for (const auto &comp : state.components)
    {
      out << "static RegCompSpec<" << comp.type << "> _reg_comp_" << escape_name(comp.name) << "(\"" << comp.name << "\");" << std::endl;
      out << "template <> int RegCompSpec<" << comp.type << ">::ID = -1;" << std::endl;
      out << std::endl;
    }

    for (const auto &ev : state.events)
    {
      out << "static RegCompSpec<" << ev.type << "> _reg_event_" << escape_name(ev.name) << ";" << std::endl;
      out << "int RegCompSpec<" << ev.type << ">::ID = -1;" << std::endl;
      out << std::endl;
    }

    auto joinParameters = [](const eastl::vector<VisitorState::Parameter> &parameters)
    {
      return utils::join(parameters,
        [](eastl::vector<VisitorState::Parameter>::const_iterator i)
      {
        return i->type + " " + i->name;
      }, ", ");
    };

    auto joinParametersFrom = [](const eastl::vector<VisitorState::Parameter> &parameters, int from)
    {
      return utils::join<VisitorState::Parameter>(parameters.begin() + from, parameters.end(),
        [](eastl::vector<VisitorState::Parameter>::const_iterator i)
      {
        return i->type + " " + i->name;
      }, ", ");
    };

    auto joinParameterNames = [](const eastl::vector<VisitorState::Parameter> &parameters)
    {
      return utils::join(parameters,
        [](eastl::vector<VisitorState::Parameter>::const_iterator i)
      {
        return "\"" + i->name + "\"";
      }, ", ");
    };

    auto joinParameterNamesFrom = [](const eastl::vector<VisitorState::Parameter> &parameters, int from)
    {
      return utils::join<VisitorState::Parameter>(parameters.begin() + from, parameters.end(),
        [](eastl::vector<VisitorState::Parameter>::const_iterator i)
      {
        return "\"" + i->name + "\"";
      }, ", ");
    };

    for (auto &sys : state.systems)
    {
      sys.fromQuery = false;
      for (int i = 1; i < (int)sys.parameters.size(); ++i)
      {
        auto &p = sys.parameters[i];
        auto res = eastl::find_if(state.queries.cbegin(), state.queries.cend(), [&](const VisitorState::Query &q) { return q.name == p.pureType; });
        if (res != state.queries.cend())
        {
          p.queryId = eastl::distance(state.queries.cbegin(), res);
          sys.fromQuery = true;
        }
        else if (sys.fromQuery)
        {
          assert(false);
          sys.fromQuery = false;
        }
      }

      if (sys.fromQuery)
      {
        continue;
      }

      if (sys.parameters.size() > 1)
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_components[] = {" << std::endl;
        for (int i = 1; i < (int)sys.parameters.size(); ++i)
        {
          const auto &p = sys.parameters[i];
          out << "  {hash::cstr(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }

      if (!sys.have.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_have_components[] = {" << std::endl;
        for (const auto &p : sys.have)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : sys.notHave)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.trackTrue.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_is_true_components[] = {" << std::endl;
        for (const auto &p : sys.trackTrue)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.trackFalse.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_is_false_components[] = {" << std::endl;
        for (const auto &p : sys.trackFalse)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }

      out << "static constexpr ConstQueryDesc " << sys.name << "_query_desc = {" << std::endl;
      if (sys.parameters.size() <= 1) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_components)," << std::endl;
      if (sys.have.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_have_components)," << std::endl;
      if (sys.notHave.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_not_have_components)," << std::endl;
      if (sys.trackTrue.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_is_true_components)," << std::endl;
      if (sys.trackFalse.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_is_false_components)," << std::endl;
      out << "};" << std::endl;
    }

    for (const auto &sys : state.systems)
    {
      if (sys.version != 1)
        continue;

      if (sys.version == 1)
        out << "static RegSysSpec<" << hash::fnv1<uint32_t>::hash(sys.name.c_str()) << ", decltype(" << sys.name << ")> _reg_sys_" << sys.name << "(\"" << sys.name << "\"";
      else
        out << "static RegSys _reg_sys_" << sys.name << "(\"" << sys.name << "\", &" << sys.name << "_run";

      if (sys.fromQuery)
        out << ");" << std::endl;
      else
        out << ", " << sys.name << "_query_desc);" << std::endl;
    }

    if (!state.systems.empty())
      out << std::endl;

    for (auto &q : state.queries)
      if (q.parameters.empty())
      {
        q.empty = true;
        auto &p = q.parameters.emplace_back();
        p.name = "eid";
        p.type = "EntityId";
        p.pureType = "EntityId";
      }

    for (const auto &q : state.queries)
    {
      out << "static constexpr ConstCompDesc " << q.name << "_components[] = {" << std::endl;
      for (const auto &p : q.parameters)
      {
        out << "  {hash::cstr(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
      }
      out << "};" << std::endl;

      if (!q.have.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_have_components[] = {" << std::endl;
        for (const auto &p : q.have)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : q.notHave)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.trackTrue.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_true_components[] = {" << std::endl;
        for (const auto &p : q.trackTrue)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.trackFalse.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_false_components[] = {" << std::endl;
        for (const auto &p : q.trackFalse)
        {
          out << "  {hash::cstr(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }

      out << "static constexpr ConstQueryDesc " << q.name << "_query_desc = {" << std::endl;
      out << "  make_const_array(" << q.name << "_components)," << std::endl;
      if (q.have.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << q.name << "_have_components)," << std::endl;
      if (q.notHave.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << q.name << "_not_have_components)," << std::endl;
      if (q.trackTrue.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << q.name << "_is_true_components)," << std::endl;
      if (q.trackFalse.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << q.name << "_is_false_components)," << std::endl;
      out << "};" << std::endl;
    }

    out << std::endl;

    for (const auto &q : state.queries)
    {
      if (q.version == 1)
        out << "static RegQuery _reg_query_" << q.name << "(hash::cstr(\"" << basename << "_" << q.name << "\"), " << q.name << "_query_desc);" << std::endl;
      else
        out << "static RegQuery _reg_query_" << q.name << "(hash::cstr(\"" << basename << "_" << q.name << "\"), " << q.name << "_query_desc, " << (q.filter.empty() ? "nullptr" : q.filter) << ");" << std::endl;
    }

    out << std::endl;

    for (const auto &q : state.queries)
    {
      if (q.version == 1)
      {
        out << "template <typename C> void " << q.name << "::exec(C callback)" << std::endl;
        out << "{" << std::endl;
        out << "  auto &query = *g_mgr->getQueryByName(hash::cstr(\"" << basename << "_" << q.name << "\"));" << std::endl;
        for (int i = 0; i < (int)q.parameters.size(); ++i)
        {
          const auto &p = q.parameters[i];
          out << "  GET_COMPONENT_INDEX(" << q.name << ", " << p.name << ");" << std::endl;
        }
        out << "  for (int chunkIdx = 0; chunkIdx < query.chunksCount; ++chunkIdx)" << std::endl;
        out << "  {" << std::endl;

        for (int i = 0; i < (int)q.parameters.size(); ++i)
        {
          const auto &p = q.parameters[i];
          out << "    GET_COMPONENT_VALUE_ITER(" << p.name << ", " << p.pureType << ");" << std::endl;
        }

        out << "    for (int i = 0; i < query.entitiesInChunk[chunkIdx]; ++i";
        for (int i = 0; i < (int)q.parameters.size(); ++i)
        {
          const auto &p = q.parameters[i];
          out << ", ++it_" << p.name;
        }
        out << ")" << std::endl;
        out << "      callback(";

        for (int i = 0; i < (int)q.parameters.size(); ++i)
        {
          const auto &p = q.parameters[i];
          if (i != 0)
            out << ", ";
          out << "*it_" << p.name;
        }

        out << ");" << std::endl;

        out << "  }" << std::endl;
        out << "}\n" << std::endl;
      }
      else
      {
        out << "template <typename Callable> void " << q.name << "::foreach(Callable callback)\n";
        out << "{\n";
        out << "  Query &query = *g_mgr->getQueryByName(hash::cstr(\"" << basename << "_" << q.name << "\"));\n";
        out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";
        out << "    callback(\n";
        out << "    {\n      ";
        for (int i = 0; i < (int)q.parameters.size(); ++i)
        {
          const auto &p = q.parameters[i];
          if (i != 0)
            out << ",\n      ";
          out << "GET_COMPONENT(" << q.name << ", q, " << p.pureType << ", " << p.name << ")";
        }
        out << "\n    });" << std::endl;
        out << "}\n";
      }
    }

    for (const auto &sys : state.systems)
    {
      if (sys.fromQuery)
        out << "static void " << sys.name << "_run(const RawArg &stage_or_event, Query&)" << std::endl;
      else if (sys.version == 1)
        out << "template <> __forceinline void RegSysSpec<" << hash::fnv1<uint32_t>::hash(sys.name.c_str()) << ", decltype(" << sys.name << ")>::execImpl(const RawArg &stage_or_event, Query &query) const" << std::endl;
      else
        out << "static void " << sys.name << "_run(const RawArg &stage_or_event, Query &query)" << std::endl;
      out << "{" << std::endl;

      if (sys.fromQuery)
      {
        assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

        if (sys.parameters.size() >= 3)
        {
          for (int i = 1; i < (int)sys.parameters.size(); ++i)
          {
            const auto &q = state.queries[sys.parameters[i].queryId];
            out << "  Query &query" << i << " = *g_mgr->getQueryByName(hash::cstr(\"" << basename << "_" << q.name << "\"));" << std::endl;
          }

          std::string indent = "  ";
          for (int i = 1; i < (int)sys.parameters.size(); ++i)
          {
            const auto &q = state.queries[sys.parameters[i].queryId];

            out << indent << "for (auto q" << i << " = query" << i << ".begin(), e = query" << i << ".end(); q" << i << " != e; ++q" << i << ")\n";
            out << indent << "{\n";
            out << indent << "  " << q.name << " " << sys.parameters[i].name << " =\n";
            out << indent << "  {\n";

            for (int j = 0; j < (int)q.parameters.size(); ++j)
            {
              const auto &p = q.parameters[j];
              if (j != 0)
                out << ",\n";
              out << indent << "    GET_COMPONENT(" << q.name << ", q"<< i << ", " << p.pureType << ", " << p.name << ")";
            }

            out << "\n";
            out << indent << "  };\n";

            indent += "  ";
          }

          if (!sys.filter.empty())
          {
            out << indent << "if (" << sys.filter << ")\n";
            indent += "  ";
          }

          out << indent << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem";
          for (int i = 1; i < (int)sys.parameters.size(); ++i)
          {
            out << ", eastl::move(" << sys.parameters[i].name << ")";
          }
          out << ");\n";

          for (int i = 1; i < (int)sys.parameters.size(); ++i)
          {
            indent.erase(indent.length() - 2, 2);
            out << indent << "}\n";
          }
        }
        else
        {
          const auto &q = state.queries[sys.parameters[1].queryId];

          out << "  Query &query = *g_mgr->getQueryByName(hash::cstr(\"" << basename << "_" << q.name << "\"));" << std::endl;
          out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";

          out << "    " << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem,\n    {\n      ";
          for (int i = 0; i < (int)q.parameters.size(); ++i)
          {
            const auto &p = q.parameters[i];
            if (i != 0)
              out << ",\n      ";
            out << "GET_COMPONENT(" << q.name << ", q, " << p.pureType << ", " << p.name << ")";
          }
          out << "\n    });" << std::endl;
        }
      }
      else
      {
        out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";

        if (sys.version == 1)
          out << "    " << sys.name << "(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem";
        else
          out << "    " << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem";
        for (int i = 1; i < (int)sys.parameters.size(); ++i)
        {
          const auto &p = sys.parameters[i];
          out << ",\n      GET_COMPONENT(" << sys.name << ", q, " << p.pureType << ", " << p.name << ")";
        }
        out << ");" << std::endl;
      }
      out << "}\n";

      if (sys.version != 1)
      {
        out << "static RegSys _reg_sys_" << sys.name << "(\"" << sys.name << "\", &" << sys.name << "_run, \"" << sys.parameters[0].pureType << "\"";

        if (sys.fromQuery)
          out << ");" << std::endl;
        else
          out << ", " << sys.name << "_query_desc);" << std::endl;
      }

      out << "\n";
    }

    out << "#endif // __CODEGEN__" << std::endl;
  }

  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);

  if (argc >= 3 && ::strcmp(argv[2], "stop") == 0)
  {
    std::cout << "Done! Press any key." << std::endl;
    std::cin.get();
  }

  return 0;
}

