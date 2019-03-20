#include "stdafx.h"

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>
#include <fmt/ostream.h>

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
      out << fmt::format("static RegCompSpec<{type}> _reg_comp_{name}(\"{name}\");\n",
        fmt::arg("type", comp.type),
        fmt::arg("name", comp.name));

      out << fmt::format("template <> int RegCompSpec<{type}>::ID = -1;\n",
        fmt::arg("type", comp.type));

      out << "\n";
    }

    for (const auto &ev : state.events)
    {
      out << fmt::format("static RegCompSpec<{type}> _reg_event_{name};\n",
        fmt::arg("type", ev.type),
        fmt::arg("name", escape_name(ev.name)));

      out << fmt::format("int RegCompSpec<{type}>::ID = -1;\n",
        fmt::arg("type", ev.type));

      out << "\n";
    }

    for (auto &sys : state.systems)
    {
      sys.fromQuery = false;
      for (int i = 1; i < (int)sys.parameters.size(); ++i)
      {
        auto &p = sys.parameters[i];
        eastl::string queryName = p.pureType;
        if (utils::startsWith(p.pureType, "QueryIterable"))
          queryName = p.templateRef;
        auto res = eastl::find_if(state.queries.cbegin(), state.queries.cend(), [&](const VisitorState::Query &q) { return q.name == queryName; });
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
      if (!sys.track.empty())
      {
        out << "static constexpr ConstCompDesc " << sys.name << "_track_components[] = {" << std::endl;
        for (const auto &p : sys.track)
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
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
      if (sys.track.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << sys.name << "_track_components)," << std::endl;
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
        out << "  {HASH(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
      out << "};" << std::endl;

      if (!q.have.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_have_components[] = {" << std::endl;
        for (const auto &p : q.have)
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        out << "};" << std::endl;
      }
      if (!q.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : q.notHave)
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        out << "};" << std::endl;
      }
      if (!q.trackTrue.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_true_components[] = {" << std::endl;
        for (const auto &p : q.trackTrue)
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        out << "};" << std::endl;
      }
      if (!q.trackFalse.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_is_false_components[] = {" << std::endl;
        for (const auto &p : q.trackFalse)
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
        out << "};" << std::endl;
      }
      if (!q.track.empty())
      {
        out << "static constexpr ConstCompDesc " << q.name << "_track_components[] = {" << std::endl;
        for (const auto &p : q.track)
          out << "  {HASH(\"" << p.name << "\"), Desc<bool>::Size}," << std::endl;
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
      if (q.track.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << q.name << "_track_components)," << std::endl;
      out << "};" << std::endl;
    }

    for (const auto &q : state.queries)
    {
      out << fmt::format("using {name}Builder = StructBuilder<\n", fmt::arg("name", q.name));
      bool addComma = false;
      for (const auto &p :  q.parameters)
      {
        if (addComma)
          out << ",\n";
        addComma = true;
        out << fmt::format("  StructField<{type}, INDEX_OF_COMPONENT({query}, {component})>",
          fmt::arg("type", p.pureType),
          fmt::arg("query", q.name),
          fmt::arg("component", p.name));
      }
      out << "\n>;\n";
    }

    for (const auto &i : state.indices)
    {
      out << "static constexpr ConstCompDesc " << i.name << "_components[] = {" << std::endl;
      for (const auto &p : i.parameters)
        out << "  {HASH(\"" << p.name << "\"), Desc<" << p.pureType << ">::Size}," << std::endl;
      out << "};" << std::endl;

      if (!i.have.empty())
      {
        out << "static constexpr ConstCompDesc " << i.name << "_have_components[] = {" << std::endl;
        for (const auto &p : i.have)
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        out << "};" << std::endl;
      }
      if (!i.notHave.empty())
      {
        out << "static constexpr ConstCompDesc " << i.name << "_not_have_components[] = {" << std::endl;
        for (const auto &p : i.notHave)
          out << "  {HASH(\"" << p.name << "\"), 0}," << std::endl;
        out << "};" << std::endl;
      }

      out << "static constexpr ConstQueryDesc " << i.name << "_query_desc = {" << std::endl;
      out << "  make_const_array(" << i.name << "_components)," << std::endl;
      if (i.have.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << i.name << "_have_components)," << std::endl;
      if (i.notHave.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << i.name << "_not_have_components)," << std::endl;
      if (i.trackTrue.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << i.name << "_is_true_components)," << std::endl;
      if (i.trackFalse.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << i.name << "_is_false_components)," << std::endl;
      if (i.track.empty()) out << "  empty_desc_array," << std::endl;
      else out << "  make_const_array(" << i.name << "_track_components)," << std::endl;
      out << "};" << std::endl;
    }

    out << std::endl;

    for (const auto &q : state.queries)
      out << "static RegQuery _reg_query_" << q.name << "(HASH(\"" << basename << "_" << q.name << "\"), " << q.name << "_query_desc, " << (q.filter.empty() ? "nullptr" : q.filter) << ");" << std::endl;

    out << std::endl;

    for (const auto &i : state.indices)
      out << "static RegIndex _reg_index_" << i.name << "(HASH(\"" << basename << "_" << i.name << "\"), " << "HASH(\"" << i.componentName << "\"), " << i.name << "_query_desc, " << (i.filter.empty() ? "nullptr" : i.filter) << ");" << std::endl;

    out << std::endl;

    for (const auto &q : state.queries)
    {
      out << "int " << q.name << "::count()\n";
      out << "{\n";
      out << "  Query &query = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << q.name << "\"));\n";
      out << "  return query.entitiesCount;\n";
      out << "}\n";

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

      out << "Index* " << q.name << "::index()\n";
      out << "{\n";
      if (q.indexId >= 0)
      {
        const auto &i = state.indices[q.indexId];
        out << "  return g_mgr->getIndexByName(HASH(\"" << basename << "_" << i.name << "\"));\n";
      }
      else
        out << "  return nullptr;\n";
      out << "}\n";

      out << q.name << " " << q.name << "::get(Query::AllIterator &iter)\n";
      out << "{\n";
      out << "  return {\n      ";
      for (int i = 0; i < (int)q.parameters.size(); ++i)
      {
        const auto &p = q.parameters[i];
        if (i != 0)
          out << ",\n      ";
        out << "GET_COMPONENT(" << q.name << ", iter, " << p.pureType << ", " << p.name << ")";
      }
      out << "\n    };" << std::endl;
      out << "}\n";
    }

    decltype(state.systems) systemsExternalQuery;
    decltype(state.systems) systemsInternalQuery;
    decltype(state.systems) systemsJoinQueries;
    decltype(state.systems) systemsJoinIndexQueries;
    decltype(state.systems) systemsGroupBy;
    decltype(state.systems) systemsQueryIterable;

    for (const auto &sys : state.systems)
    {
      if (sys.fromQuery)
      {
        if (sys.parameters.size() >= 3)
        {
          if (sys.indexId >= 0)
          {
            if (!sys.parameters[2].templateRef.empty())
              systemsGroupBy.push_back(sys);
            else
              systemsJoinIndexQueries.push_back(sys);
          }
          else
            systemsJoinQueries.push_back(sys);
        }
        else if (sys.parameters.size() >= 2 && !sys.parameters[1].templateRef.empty())
          systemsQueryIterable.push_back(sys);
        else
          systemsExternalQuery.push_back(sys);
      }
      else
        systemsInternalQuery.push_back(sys);
    }

    for (const auto &sys : systemsQueryIterable)
    {
      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

      const auto &query = state.queries[sys.parameters[1].queryId];

      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query&)\n", fmt::arg("system", sys.name));
      out << "{\n";

      out << fmt::format("  Query &query = *g_mgr->getQueryByName(HASH(\"{basename}_{query}\"));\n",
        fmt::arg("basename", basename),
        fmt::arg("query", query.name));

      out << fmt::format("  {system}::run(*({stage}*)stage_or_event.mem, QueryIterable<{query}, {query}Builder>(query));\n",
        fmt::arg("system", sys.name),
        fmt::arg("query", query.name),
        fmt::arg("stage", sys.parameters[0].pureType));

      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\");\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType));
    }

    for (const auto &sys : systemsGroupBy)
    {
      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

      const auto &query = state.queries[sys.parameters[2].queryId];
      const auto &index =  state.indices[sys.indexId];

      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query&)\n", fmt::arg("system", sys.name));
      out << "{\n";

      out << fmt::format("  Index &index = *g_mgr->getIndexByName(HASH(\"{basename}_{index}\"));\n",
        fmt::arg("basename", basename),
        fmt::arg("index", index.name));

      out << "  for (const Index::Item &item : index.items)\n";
      out << fmt::format("    {system}::run(*({stage}*)stage_or_event.mem, *({indexType}*)(uint8_t*)&item.value, QueryIterable<{query}, {query}Builder>(index.queries[item.queryId]));\n",
        fmt::arg("system", sys.name),
        fmt::arg("query", query.name),
        fmt::arg("stage", sys.parameters[0].pureType),
        fmt::arg("indexType", sys.parameters[1].pureType));

      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\");\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType));
    }

    for (const auto &sys : systemsJoinIndexQueries)
    {
      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

      const auto &index =  state.indices[sys.indexId];

      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query&)\n", fmt::arg("system", sys.name));
      out << "{\n";

      out << "  Index &index = *g_mgr->getIndexByName(HASH(\"" << basename << "_" << index.name << "\"));\n";

      const auto &q1 = state.queries[sys.parameters[1].queryId];
      out << "  Query &query1 = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << q1.name << "\"));" << std::endl;

      std::string indent = "  ";
      for (int i = 1; i < (int)sys.parameters.size(); ++i)
      {
        const auto &q = state.queries[sys.parameters[i].queryId];

        if (i == 1)
        {
          out << indent << "for (auto q" << i << " = query" << i << ".begin(), e = query" << i << ".end(); q" << i << " != e; ++q" << i << ")\n";
          out << indent << "{\n";
        }
        else
        {
          out << indent << "if (Query *pquery" << i << " = index.find(*(uint32_t*)(uint8_t*)&" << index.lookup << "))\n";
          out << indent << "{\n";
          out << indent << "  Query &query" << i << " = *pquery" << i << ";\n";
          out << indent << "  for (auto q" << i << " = query" << i << ".begin(), e = query" << i << ".end(); q" << i << " != e; ++q" << i << ")\n";
          out << indent << "  {\n";
          indent += "  ";
        }
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
        if (i != 1)
        {
          indent.erase(indent.length() - 2, 2);
          out << indent << "}\n";
        }
      }

      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\");\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType));
    }

    for (const auto &sys : systemsJoinQueries)
    {
      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query&)\n", fmt::arg("system", sys.name));
      out << "{\n";

      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

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

      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\");\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType));
    }

    for (const auto &sys : systemsExternalQuery)
    {
      assert(sys.parameters.size() >= 2 && sys.parameters.size() <= 3);

      const auto &query = state.queries[sys.parameters[1].queryId];

      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query&)\n", fmt::arg("system", sys.name));
      out << "{\n";

      out << "  Query &query = *g_mgr->getQueryByName(HASH(\"" << basename << "_" << query.name << "\"));" << std::endl;
      out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";

      out << "    " << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem,\n    {\n      ";
      for (int i = 0; i < (int)query.parameters.size(); ++i)
      {
        const auto &p = query.parameters[i];
        if (i != 0)
          out << ",\n      ";
        out << "GET_COMPONENT(" << query.name << ", q, " << p.pureType << ", " << p.name << ")";
      }
      out << "\n    });" << std::endl;

      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\");\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType));
    }

    for (const auto &sys : systemsInternalQuery)
    {
      out << fmt::format("static void {system}_run(const RawArg &stage_or_event, Query &query)\n", fmt::arg("system", sys.name));
      out << "{\n";

      out << "  for (auto q = query.begin(), e = query.end(); q != e; ++q)\n";
      out << "    " << sys.name << "::run(*(" << sys.parameters[0].pureType << "*)stage_or_event.mem";
      for (int i = 1; i < (int)sys.parameters.size(); ++i)
      {
        const auto &p = sys.parameters[i];
        out << ",\n      GET_COMPONENT(" << sys.name << ", q, " << p.pureType << ", " << p.name << ")";
      }
      out << ");" << std::endl;
      out << "}\n";

      out << fmt::format("static RegSys _reg_sys_{system}(\"{system}\", &{system}_run, \"{stage}\", {system}_query_desc, {filter});\n\n",
        fmt::arg("system", sys.name),
        fmt::arg("stage", sys.parameters[0].pureType),
        fmt::arg("filter", sys.filter.empty() ? "nullptr" : sys.filter));
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

