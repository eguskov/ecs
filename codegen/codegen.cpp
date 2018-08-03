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
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &parameters);
}

int main(int argc, char* argv[])
{
  std::cout << "Codegen: " << argv[1] << std::endl;

  char path[MAX_PATH];
  ::GetCurrentDirectory(MAX_PATH, path);

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
    "-I../rapidjson/include",
    "-I../ecs",
    "-I../glm",
    "-I../EASTL/include",
    "-I../EASTL/test/packages/EABase/include/Common",
    "-I../raylib/src",
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

    struct System : Function
    {
    };

    struct Query : Function
    {
      CXCursor cursor;
      eastl::string components;
    };

    struct Component
    {
      eastl::string name;
      eastl::string type;
    };

    eastl::string filename;

    CXTranslationUnit unit;

    eastl::vector<System> systems;
    eastl::vector<Query> queries;
    eastl::vector<Component> components;
  } state;

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    VisitorState &state = *static_cast<VisitorState*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursorKind parentKind = clang_getCursorKind(parent);

    ScopeString spelling = clang_getCursorSpelling(cursor);
    ScopeString kindSpelling = clang_getCursorKindSpelling(kind);

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl)
    {
      auto structVisitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
      {
        VisitorState &state = *static_cast<VisitorState*>(data);

        if (!clang_Location_isFromMainFile(clang_getCursorLocation(parent)))
          return CXChildVisit_Continue;

        CXCursorKind kind = clang_getCursorKind(cursor);

        if (kind == CXCursor_AnnotateAttr)
        {
          auto name = to_string(clang_getCursorSpelling(cursor));
          auto structName = to_string(clang_getCursorSpelling(parent));

          if (utils::startsWith(name, "@component: "))
          {
            CXFile f1 = clang_getFile(state.unit, state.filename.c_str());
            CXFile f2 = clang_getIncludedFile(cursor);

            auto &comp = state.components.push_back();
            comp.type = eastl::move(structName);
            comp.name = name.substr(::strlen("@component: "));
          }
          else if (name == "@query")
          {
            auto &q = state.queries.push_back();
            q.name = eastl::move(structName);
          }
        }

        return CXChildVisit_Continue;
      };

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
        }
        else if (kind == CXCursor_CompoundStmt)
        {
          eastl::string funcName = to_string(clang_getCursorSpelling(parent));
          auto it = eastl::find_if(state.systems.begin(), state.systems.end(), [&](const VisitorState::Function &f) { return f.name == funcName; });
          if (it != state.systems.end())
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
                    if (it != parentState.queries.end())
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
  clang_visitChildren(cursor, visitor, &state);
  CXFile unitFile = clang_getFile(unit, argv[1]);

  std::cerr << "Done:" << std::endl;
  std::cerr << "  systems: " << state.systems.size() << std::endl;
  std::cerr << "  queries: " << state.queries.size() << std::endl;
  std::cerr << "  components: " << state.components.size() << std::endl;

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

    out << "//! GENERATED FILE\n\n" << std::endl;
    out << "#ifndef __CODEGEN__\n" << std::endl;

    out << "#include \"" << basename << "\"\n" << std::endl;

    for (const auto &comp : state.components)
    {
      out << "static RegCompSpec<" << comp.type << "> _reg_comp_" << comp.name << "(\"" << comp.name << "\");" << std::endl;
      out << "template <> int RegCompSpec<" << comp.type << ">::ID = -1;" << std::endl;
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

    auto joinParameterNames = [](const eastl::vector<VisitorState::Parameter> &parameters)
    {
      return utils::join(parameters,
        [](eastl::vector<VisitorState::Parameter>::const_iterator i)
      {
        return "\"" + i->name + "\"";
      }, ", ");
    };

    auto seq = [](int count)
    {
      eastl::vector<int> vec;
      vec.resize(count);
      for (int i = 0; i < count; ++i)
        vec[i] = i;

      return utils::join(vec, ", ");
    };

    auto argIdSeq = [](int count)
    {
      eastl::vector<eastl::string> vec;
      vec.resize(count);
      for (int i = 0; i < count; ++i)
      {
        std::ostringstream oss;
        // oss << "RegCompSpec<Argument<" << i << ">::PureType>::ID";
        oss << "components[" << i << "].nameId";
        vec[i].assign(oss.str().c_str());
      }

      return utils::join(vec, ", ");
    };

    auto argOffsetSeq = [](int count, const char *prefix = "")
    {
      eastl::vector<eastl::string> vec;
      vec.resize(count);
      for (int i = 0; i < count; ++i)
      {
        std::ostringstream oss;
        oss << "get_offset(remap[" << i << "], offsets)";
        vec[i].assign(oss.str().c_str());
      }

      return utils::join(vec, ", ");
    };

    auto valuesSeq = [](int count, const char *prefix = "")
    {
      eastl::vector<eastl::string> vec;
      vec.resize(count);
      for (int i = 0; i < count; ++i)
      {
        std::ostringstream oss;
        oss << "ValueSoA<" << prefix << "Argument<" << i << ">::Type, " << prefix << "Argument<" << i << ">::valueType>::get(args, storage, argId[" << i << "], argOffset[" << i << "])";
        vec[i].assign(oss.str().c_str());
      }

      return utils::join(vec, ", ");
    };

    for (const auto &sys : state.systems)
    {
      out << "template <> struct Desc<decltype(" << sys.name << ")> { constexpr static char* name = \"" << sys.name << "\"; };" << std::endl;
      out << "static RegSysSpec<decltype(" << sys.name << ")> _reg_sys_" << sys.name << "(\"" << sys.name << "\", " << sys.name << ", { ";

      out << joinParameterNames(sys.parameters);

      out << " }, true);" << std::endl;
      out << std::endl;
    }

    for (const auto &q : state.queries)
    {
      out << "static __forceinline void exec_" << q.name << "(" << joinParameters(q.parameters) << ") {}\n" << std::endl;

      out << "template <> struct Desc<decltype(exec_" << q.name << ")> { constexpr static char* name = \"exec_" << q.name << "\"; };" << std::endl;
      out << "static RegSysSpec<decltype(exec_" << q.name << ")> _reg_sys_exec_" << q.name << "(\"exec_" << q.name << "\", exec_" << q.name << ", { ";

      out << joinParameterNames(q.parameters);

      out << " }, false);" << std::endl;
      out << std::endl;

      out << "template <> template <> __forceinline void RegSysSpec<decltype(exec_" << q.name << ")>::execImplSoA<>(const ExtraArguments &args, const RegSys::Remap &remap, const int *offsets, Storage *storage, eastl::index_sequence<" << seq(q.parameters.size()) << ">) const {}\n" << std::endl;

      out << "template <typename C> void " << q.name << "::exec(C callback)" << std::endl;
      out << "{" << std::endl;
      out << "  using SysType = RegSysSpec<decltype(exec_" << q.name << ")>;" << std::endl;
      out << "  const auto &sys = _reg_sys_exec_" << q.name << ";" << std::endl;
      out << "  const auto &components = sys.components;" << std::endl;
      out << "  const auto &query = g_mgr->queries[sys.id];" << std::endl;
      out << "  auto *storage = &g_mgr->storagesSoA[0];" << std::endl;
      out << "  ExtraArguments args;" << std::endl;
      out << "  for (int i = 0; i < (int)query.eids.size(); ++i)" << std::endl;
      out << "  {" << std::endl;
      out << "    EntityId eid = query.eids[i];" << std::endl;

      out << "    args.eid = eid;" << std::endl;

      out << "    const auto &entity = g_mgr->entitiesSoA[eid2idx(eid)];" << std::endl;
      out << "    const auto &templ = g_mgr->templates[entity.templateId];" << std::endl;
      out << "    const auto &remap = templ.remaps[sys.id];" << std::endl;
      out << "    const int *offsets = entity.componentOffsets.data();" << std::endl;

      out << "    const int argId[] = { " << argIdSeq(q.parameters.size()) << " };" << std::endl;
      out << "    const int argOffset[] = { " << argOffsetSeq(q.parameters.size()) << " };" << std::endl;

      out << "    callback(" << valuesSeq(q.parameters.size(), "SysType::") << ");" << std::endl;

      out << "  }" << std::endl;
      out << "}\n" << std::endl;
    }

    for (const auto &sys : state.systems)
    {
      out << "template <> template <> __forceinline void RegSysSpec<decltype(" << sys.name << ")>::execImplSoA<>(const ExtraArguments &args, const RegSys::Remap &remap, const int *offsets, Storage *storage, eastl::index_sequence<" << seq(sys.parameters.size()) << ">) const" << std::endl;
      out << "{" << std::endl;
      out << "  const int argId[] = { " << argIdSeq(sys.parameters.size()) << " };" << std::endl;
      out << "  const int argOffset[] = { " << argOffsetSeq(sys.parameters.size()) << " };" << std::endl;
      out << "  " << sys.name << "(";
      out << valuesSeq(sys.parameters.size());
      out << ");" << std::endl;
      out << "}\n" << std::endl;
    }

    out << "#endif // __CODEGEN__" << std::endl;
  }

  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);

  return 0;
}

