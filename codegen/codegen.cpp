#include "stdafx.h"

#include <Windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <regex>

#include <EASTL/functional.h>

#include "utils.h"
#include "parser.h"
#include "queryparser.h"

eastl::string escape_name(const eastl::string name)
{
  return std::regex_replace(name.c_str(), std::regex("::"), "_").c_str();
}

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
  state.unit = unit;
  state.filename = argv[1];

  parse(unit, state);

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
          out << "  {HASH(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }

      if (!sys.have.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_have_components[] = {" << std::endl;
        for (const auto &p : sys.have)
        {
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : sys.notHave)
        {
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.trackTrue.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_is_true_components[] = {" << std::endl;
        for (const auto &p : sys.trackTrue)
        {
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!sys.trackFalse.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_is_false_components[] = {" << std::endl;
        for (const auto &p : sys.trackFalse)
        {
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
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
        out << "  {HASH(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
      }
      out << "};" << std::endl;

      if (!q.have.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_have_components[] = {" << std::endl;
        for (const auto &p : q.have)
        {
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : q.notHave)
        {
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.trackTrue.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_true_components[] = {" << std::endl;
        for (const auto &p : q.trackTrue)
        {
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        }
        out << "};" << std::endl;
      }
      if (!q.trackFalse.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_false_components[] = {" << std::endl;
        for (const auto &p : q.trackFalse)
        {
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
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
      out << "static RegQuery _reg_query_" << q.name << "(HASH(\"" << basename << "_" << q.name << "\"), " << q.name << "_query_desc, " << (q.filter.empty() ? "nullptr" : q.filter) << ");" << std::endl;

    out << std::endl;

    for (const auto &q : state.queries)
    {
      out << "template <typename Callable> void " << q.name << "::foreach(Callable callback)\n";
      out << "{\n";
      out << "  Query &query = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << q.name << "\"));\n";
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

    for (const auto &sys : state.systems)
    {
      if (!sys.fromQuery)
        continue;

      out << "static void " << sys.name << "_run(const RawArg &stage_or_event, Query&)" << std::endl;
      out << "{" << std::endl;

      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

      if (sys.parameters.size() >= 3)
      {
        for (int i = 1; i < (int)sys.parameters.size(); ++i)
        {
          const auto &q = state.queries[sys.parameters[i].queryId];
          out << "  Query &query" << i << " = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << q.name << "\"));" << std::endl;
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

        out << "  Query &query = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << q.name << "\"));" << std::endl;
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

      out << "}\n";

      out << "static RegSys _reg_sys_" << sys.name << "(\"" << sys.name << "\", &" << sys.name << "_run, \"" << sys.parameters[0].pureType << "\"";

      out << ");\n\n";
    }

    for (const auto &sys : state.systems)
    {
      if (sys.fromQuery)
        continue;

      out << "static void " << sys.name << "_run(const RawArg &stage_or_event, Query &query)" << std::endl;
      out << "{" << std::endl;
      out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";
      out << "    " << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem";
      for (int i = 1; i < (int)sys.parameters.size(); ++i)
      {
        const auto &p = sys.parameters[i];
        out << ",\n      GET_COMPONENT(" << sys.name << ", q, " << p.pureType << ", " << p.name << ")";
      }
      out << ");" << std::endl;
      out << "}\n";

      out << "static RegSys _reg_sys_" << sys.name << "(\"" << sys.name << "\", &" << sys.name << "_run, \"" << sys.parameters[0].pureType << "\"";
      out << ", " << sys.name << "_query_desc);\n\n";
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

