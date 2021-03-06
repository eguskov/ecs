#include "parser.h"

#include <iostream>
#include <ostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <regex>

#include <EASTL/algorithm.h>

#include "utils.h"
#include "queryparser.h"

struct DumpState
{
  int level = 0;
};

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

template<typename T>
void read_template_ref(CXCursor cursor, T &ref)
{
  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    T &ref = *static_cast<T*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_TemplateRef)
    {
      ref = get_type_ref(parent);
      return CXChildVisit_Break;
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &ref);
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

      p.isRW = p.type.find("&") != eastl::string::npos && p.type.find("const ") == eastl::string::npos;

      read_template_ref(cursor, p.templateRef);
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

    if ((kind == CXCursor_CXXMethod || kind == CXCursor_FunctionTemplate) && to_string(clang_getCursorSpelling(cursor)) == state.name)
    {
      state.res = cursor;
      return CXChildVisit_Break;
    }

    return CXChildVisit_Continue;
  };

  clang_visitChildren(cursor, visitor, &state);

  return state.res;
}

eastl::vector<eastl::string> read_struct_methods(CXCursor cursor)
{
  struct State
  {
    const char *name;
    eastl::vector<eastl::string> res;
  } state;

  auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData data)
  {
    State &state = *static_cast<State*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_CXXMethod || kind == CXCursor_FunctionTemplate)
      state.res.emplace_back() = to_string(clang_getCursorSpelling(cursor));

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

      p.isRW = p.type.find("&") != eastl::string::npos && p.type.find("const ") == eastl::string::npos;

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

static CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData data)
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
        eastl::string parentName;
        eastl::string fieldName;

        auto p = clang_getCursorSemanticParent(parent);
        while (!clang_Cursor_isNull(p) && !clang_equalCursors(p, state.unitCursor))
        {
          parentName = to_string(clang_getCursorSpelling(p));
          fieldName = structName;
          structName = parentName + "::" + structName;
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
        else if (utils::startsWith(name, "@bind: "))
        {
          auto res = eastl::find_if(state.autoBind.begin(), state.autoBind.end(), [&] (const VisitorState::AutoBind &a) { return a.type == parentName; });
          if (res != state.autoBind.end())
          {
            auto flags = utils::split(utils::replace(name.substr(::strlen("@bind: ")), " ", ""), ';');
            if (clang_getCursorKind(parent) == CXCursor_CXXMethod)
            {
              VisitorState::AutoBind::Method &method = res->methods.emplace_back();
              method.name = fieldName;
              method.fullName = structName;
              for (const auto &flag : flags)
              {
                auto kv = utils::split(flag, '=');
                if (kv.empty())
                  continue;

                if (kv[0] == "name")
                {
                  method.name = kv[1];
                  method.fullName = parentName + "::" + kv[1];
                }
                else if (kv[0] == "sideEffect")
                  method.sideEffect = kv[1];
                else if (kv[0] == "isBuiltin")
                  method.isBuiltin = kv[1] == "true";
                else if (kv[0] == "simNode")
                  method.simNode = kv[1];
              }
            }
            else
            {
              const auto &flag = flags[0];
              auto kv = utils::split(flag, '=');
              if (kv.size() > 1 && kv[0] == "name")
                res->fields.push_back({fieldName, kv[1]});
              else
                res->fields.push_back({fieldName, fieldName});
            }
          }
        }
      }
      else if (kind == CXCursor_FieldDecl || kind == CXCursor_CXXMethod)
      {
        return CXChildVisit_Recurse;
      }

      return CXChildVisit_Continue;
    };

    {
      auto structName = to_string(clang_getCursorSpelling(cursor));
      if (utils::startsWith(structName, "ecs_component_"))
      {
        CXFile file;
        unsigned line;
        unsigned column;
        unsigned offset;
        clang_getFileLocation(clang_getCursorLocation(cursor), &file, &line, &column, &offset);
        if (state.filename == to_string(clang_getFileName(file)))
        {
          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);

          auto &comp = state.components.push_back();
          comp.type = std::regex_replace(structName.c_str(), std::regex("ecs_component_"), "").c_str();
          comp.name = fields[0].value;
        }
      }
      else
      {
        CXFile file;
        unsigned line;
        unsigned column;
        unsigned offset;
        clang_getFileLocation(clang_getCursorLocation(cursor), &file, &line, &column, &offset);
        if (state.filename == to_string(clang_getFileName(file)))
        {
          auto parentCursor = cursor;
          foreach_struct_decl(cursor, [&](CXCursor cursor, const eastl::string &name)
          {
            if (name == "ecs_bind")
            {
              auto res = eastl::find_if(state.autoBind.begin(), state.autoBind.end(), [&] (const VisitorState::AutoBind &a) { return a.type == structName; });
              assert(res != state.autoBind.end());

              foreach_struct_decl(cursor, [&](CXCursor cursor, const eastl::string &name)
              {
                if (utils::startsWith(name, "ecs_bind_field_"))
                {
                  eastl::vector<VisitorState::Parameter> fields;
                  read_struct_fields(cursor, fields);
                  for (const auto &f : fields)
                    res->fields.emplace_back() = {f.value, f.value};
                }
              });
            }
            else if (name == "ecs_bind_all_fields")
            {
              auto res = eastl::find_if(state.autoBind.begin(), state.autoBind.end(), [&] (const VisitorState::AutoBind &a) { return a.type == structName; });
              assert(res != state.autoBind.end());

              eastl::vector<VisitorState::Parameter> fields;
              read_struct_fields(parentCursor, fields);
              for (const auto &f : fields)
                res->fields.emplace_back() = {f.name, f.name};
            }
            else if (name == "ecs_bind_type")
            {
              eastl::vector<VisitorState::Parameter> fields;
              read_struct_fields(cursor, fields);

              auto &autoBind = state.autoBind.emplace_back();

              for (const auto &f : fields)
              {
                auto bindType = utils::split(utils::replace(f.value, " ", ""), ';');
                autoBind.module = bindType[0];
                autoBind.type = structName;

                bindType.erase(bindType.begin());
                for (const auto &flag : bindType)
                {
                  auto kv = utils::split(flag, '=');
                  if (kv[0] == "canCopy")
                    autoBind.canCopy = kv[1] == "true";

                  if (kv[0] == "canNew")
                    autoBind.canNew = kv[1] == "true";
                  else
                    autoBind.flags.push_back({ kv[0], kv[1] });
                }
              }
            }
          });
        }
      }
    }

    bool isQuery = false;
    bool isLazyQuery = false;
    bool isSystem = false;
    bool isSystemInJobs = false;
    bool isBarrier = false;
    foreach_struct_decl(cursor, [&isQuery]       (CXCursor, const eastl::string &name) { if (name == "ecs_query") isQuery = true; });
    foreach_struct_decl(cursor, [&isLazyQuery]   (CXCursor, const eastl::string &name) { if (name == "ecs_lazy_query") isLazyQuery = true; });
    foreach_struct_decl(cursor, [&isSystem]      (CXCursor, const eastl::string &name) { if (name == "ecs_system") isSystem = true; });
    foreach_struct_decl(cursor, [&isSystemInJobs](CXCursor, const eastl::string &name) { if (name == "ecs_system_in_jobs") isSystemInJobs = true; });
    foreach_struct_decl(cursor, [&isBarrier]     (CXCursor, const eastl::string &name) { if (name == "ecs_barrier") isBarrier = true; });

    if (isQuery || isLazyQuery)
    {
      auto structName = to_string(clang_getCursorSpelling(cursor));
      auto &q = state.queries.push_back();
      q.name = eastl::move(structName);
      q.lazy = isLazyQuery;

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
            {
              auto res = eastl::find_if(q.have.begin(), q.have.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == q.have.end())
                q.have.emplace_back().name = c.name;
            }
            {
              auto res = eastl::find_if(q.track.begin(), q.track.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == q.track.end())
                q.track.emplace_back().name = c.name;
            }
          }
        }
        else if (name == "ql_index")
        {
          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);

          eastl::string indexComponent = fields[0].name;
          eastl::string indexName = "index_by_" + q.name + "_" + indexComponent;

          auto indexRes = eastl::find_if(state.indices.begin(), state.indices.end(), [&] (const VisitorState::Index &i) { return i.name == indexName; });
          if (indexRes != state.indices.end())
          {
            q.indexId = eastl::distance(state.indices.begin(), indexRes);
            return;
          }

          q.indexId = state.indices.size();
          auto &i = state.indices.push_back();

          for (const auto &f : fields)
          {
            i.parameters.emplace_back().name = f.name;
            i.parameters.back().pureType = f.pureType;
            i.parameters.back().type = f.type;
          }

          i.name = indexName;
          i.componentName = indexComponent;
          i.parameters = q.parameters;
          i.have = q.have;
          i.notHave = q.notHave;
          i.filter = q.filter;

          auto componentRes = eastl::find_if(i.parameters.begin(), i.parameters.end(), [&] (const VisitorState::Parameter &p) { return p.name == indexComponent; });
          auto haveRes = eastl::find_if(i.have.begin(), i.have.end(), [&] (const VisitorState::Parameter &p) { return p.name == indexComponent; });
          if (componentRes == i.parameters.end() && haveRes == i.have.end())
          {
            auto &p = i.have.emplace_back();
            p.name = i.componentName;
          }
        }
      });
    }

    if (isSystem || isSystemInJobs || isBarrier)
    {
      auto structName = to_string(clang_getCursorSpelling(cursor));
      auto &s = state.systems.push_back();
      s.name = eastl::move(structName);
      s.inJobs = isSystemInJobs;
      s.isBarrier = isBarrier;

      if (s.inJobs)
      {
        foreach_struct_decl(cursor, [&](CXCursor cursor, const eastl::string &name)
        {
          if (name == "ecs_jobs_chunk_size")
          {
            eastl::vector<VisitorState::Parameter> fields;
            read_struct_fields(cursor, fields);
            s.chunkSize = fields[0].value;
          }
        });
      }

      CXCursor addJobsCursor = find_method(cursor, "addJobs");
      s.addJobs = !clang_equalCursors(addJobsCursor, clang_getNullCursor());

      if (!isBarrier)
      {
        CXCursor runCursor = find_method(cursor, "run");
        assert(!clang_equalCursors(runCursor, clang_getNullCursor()));

        read_function_params(runCursor, s.parameters);
      }

      for (const auto &p : s.parameters)
        if (!p.templateRef.empty() && s.parameters.size() >= 3)
        {
          s.indexId = state.indices.size();
          auto &i = state.indices.push_back();

          auto indexComponent = s.parameters[1].name;
          auto queryName = p.templateRef;
          auto res = eastl::find_if(state.queries.begin(), state.queries.end(), [&] (const VisitorState::Query &q) { return q.name == queryName; });
          assert(res != state.queries.end());

          i.name = "index_by_" + queryName + "_" + indexComponent;
          i.componentName = indexComponent;
          i.parameters = res->parameters;
          i.filter = res->filter;

          break;
        }

      CXCursor systemCursor = cursor;
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
            {
              auto res = eastl::find_if(s.have.begin(), s.have.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == s.have.end())
                s.have.emplace_back().name = c.name;
            }
            {
              auto res = eastl::find_if(s.track.begin(), s.track.end(), [&] (const VisitorState::Parameter &p) { return p.name == c.name; });
              if (res == s.track.end())
                s.track.emplace_back().name = c.name;
            }
          }
        }
        else if (name == "ql_join")
        {
          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);

          s.filter = fields[0].value;
        }
        else if (name == "ql_index")
        {
          eastl::string lookup;
          foreach_struct_decl(systemCursor, [&](CXCursor cursor, const eastl::string &name)
          {
            if (name == "ql_index_lookup")
            {
              eastl::vector<VisitorState::Parameter> fields;
              read_struct_fields(cursor, fields);
              lookup = fields[0].value;
            }
          });
          assert(!lookup.empty());

          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);

          eastl::string indexComponent = fields[0].name;
          eastl::string queryName = fields[0].pureType;
          eastl::string indexName = "index_by_" + queryName + "_" + indexComponent;

          auto indexRes = eastl::find_if(state.indices.begin(), state.indices.end(), [&] (const VisitorState::Index &i) { return i.name == indexName; });
          if (indexRes != state.indices.end())
          {
            s.indexId = eastl::distance(state.indices.begin(), indexRes);
            return;
          }

          s.indexId = state.indices.size();
          auto &i = state.indices.push_back();

          for (const auto &f : fields)
          {
            i.parameters.emplace_back().name = f.name;
            i.parameters.back().pureType = f.pureType;
            i.parameters.back().type = f.type;
          }

          auto res = eastl::find_if(state.queries.begin(), state.queries.end(), [&] (const VisitorState::Query &q) { return q.name == queryName; });
          assert(res != state.queries.end());

          i.name = indexName;
          i.componentName = indexComponent;
          i.parameters = res->parameters;
          i.filter = res->filter;
          i.lookup = lookup;
        }
        else if (name == "ecs_before")
        {
          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);
          for (const auto &f : fields)
            s.before.emplace_back() = f.value;
        }
        else if (name == "ecs_after")
        {
          eastl::vector<VisitorState::Parameter> fields;
          read_struct_fields(cursor, fields);
          for (const auto &f : fields)
            s.after.emplace_back() = f.value;
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

void parse(const CXTranslationUnit &unit, VisitorState &state)
{
  CXCursor cursor = clang_getTranslationUnitCursor(unit);
  state.unitCursor = cursor;
  clang_visitChildren(cursor, visitor, &state);

  std::cout << "Done:" << std::endl;
  std::cout << "  systems: " << state.systems.size() << std::endl;
  std::cout << "  queries: " << state.queries.size() << std::endl;
  std::cout << "  indices: " << state.indices.size() << std::endl;
  std::cout << "  components: " << state.components.size() << std::endl;
  std::cout << "  events: " << state.events.size() << std::endl;
}