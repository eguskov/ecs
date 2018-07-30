// codegen.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <clang-c\Index.h>

#include <Windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/functional.h>

namespace utils
{
  template<typename T>
  static std::string join(const std::vector<T> &v, const std::string &token) {
    std::ostringstream result;
    for (typename std::vector<T>::const_iterator i = v.begin(); i != v.end(); i++) {
      if (i != v.begin())
        result << token;
      result << *i;
    }

    return result.str();
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

int main(int argc, char* argv[])
{
  const char *comment = "//@require(Gravity gravity)";
  std::regex re = std::regex("@require\\((.*?)\\)");

  std::cmatch match;
  std::regex_search(comment, match, re);

  std::cout << match[0] << std::endl;
  std::cout << match[1] << std::endl;

  char path[MAX_PATH];
  ::GetCurrentDirectory(MAX_PATH, path);

  const char *clangArgs[] = {
    "-xc++",
    "-w",
    // "-D_DEBUG",
    "-Wno-invalid-token-paste",
    "-IC:/Users/e.guskov/Documents/Visual Studio 2015/Projects/ecs/rapidjson/include",
    "-IC:/Users/e.guskov/Documents/Visual Studio 2015/Projects/ecs/ecs",
    "-IC:/Users/e.guskov/Documents/Visual Studio 2015/Projects/ecs/glm",
    //"-ID:/Projects/Atum/Atum",
    //"-ID:/Projects/Atum/Libraries/EUI/include",
    //"-ID:/Projects/Atum/Libraries/PhysX/Include"
  };
  const int clangArgsCount = _countof(clangArgs);

  CXIndex index = clang_createIndex(1, 1);
  CXTranslationUnit unit = clang_parseTranslationUnit(
    index,
    argv[1],
    clangArgs, clangArgsCount,
    nullptr, 0,
    CXTranslationUnit_None);
  if (unit == nullptr)
  {
    std::cerr << "Unable to parse translation unit. Quitting." << std::endl;
    return -1;
  }

  struct VisitorState
  {
    struct Parameter
    {
      eastl::string name;
      eastl::string type;
    };

    struct Function
    {
      eastl::string name;
      eastl::string kind;
      eastl::string comment;

      eastl::vector<Parameter> parameters;
    };

    struct System
    {
      int functionId = -1;
    };

    eastl::vector<Function> functions;
    eastl::vector<System> systems;
  } state;

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    VisitorState *state = static_cast<VisitorState*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursorKind parentKind = clang_getCursorKind(parent);

    ScopeString spelling = clang_getCursorSpelling(cursor);
    ScopeString kindSpelling = clang_getCursorKindSpelling(kind);

    if (kind == CXCursor_FunctionDecl)
    {
      auto &func = state->functions.push_back();
      func.name = to_string(spelling);
      func.kind = to_string(kindSpelling);

      CXSourceRange range = clang_Cursor_getCommentRange(cursor);
      if (!clang_equalRanges(range, clang_getNullRange()))
      {
        func.comment = to_string(clang_Cursor_getRawCommentText(cursor));

        std::regex re = std::regex("@system");

        std::cmatch match;
        std::regex_search(func.comment.cbegin(), func.comment.cend(), match, re);
        if (!match.empty())
        {
          auto &sys = state->systems.push_back();
          sys.functionId = state->functions.size() - 1;
        }
      }
    }
    else if (kind == CXCursor_ParmDecl)
    {
      // CXCursor funcCursor = clang_getCursorSemanticParent(parent);
      eastl::string funcName = to_string(clang_getCursorSpelling(parent));
      auto funcIt = eastl::find_if(state->functions.begin(), state->functions.end(), [&](const VisitorState::Function &f) { return f.name == funcName; });
      if (funcIt != state->functions.end())
      {
        auto &p = funcIt->parameters.push_back();
        p.name = to_string(spelling);
        p.type = to_string(clang_getTypeSpelling(clang_getCursorType(cursor)));
      }
    }

    return CXChildVisit_Recurse;
  };

  CXCursor cursor = clang_getTranslationUnitCursor(unit);
  clang_visitChildren(cursor, visitor, &state);

  {
    const char *filename = "C:/Users/e.guskov/Documents/Visual Studio 2015/Projects/ecs/ecs/systems/update.gen.cpp";
    std::ofstream out;
    out.open(filename, std::ofstream::out);
    if (!out.is_open())
    {
      std::cerr << "Cannon open: " << filename << std::endl;
      return -1;
    }

    out << "#include \"update.cpp\"\n" << std::endl;

    for (const auto &sys : state.systems)
    {
      // template <> struct Desc<decltype(func)> { constexpr static char* name = #func; };
      // static RegSysSpec<decltype(func)> _##func(#func, &func, { __VA_ARGS__ });

      const auto &func = state.functions[sys.functionId];
      out << "template <> struct Desc<decltype(" << func.name << ")> { constexpr static char* name = \"" << func.name << "\"; };" << std::endl;
      out << "static RegSysSpec<decltype(" << func.name << ")> _reg_sys_" << func.name << "(\"" << func.name << "\", &" << func.name << ", { ";

      out << utils::join(func.parameters,
        [](decltype(func.parameters)::const_iterator i)
      {
        return "\"" + i->name + "\"";
      }, ", ");
      //for (const auto &p : func.parameters)
      //  out << p.name << ", ";

      out << " });" << std::endl;
      out << std::endl;
    }
  }

  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);

  return 0;
}

