options indenting = 2
// options log = true
// options log_nodes = true

require strings
require clang

require daslib/regex
require daslib/regex_boost

[init]
def init()
  pass

def advance()
  return CXChildVisitResult CXChildVisit_Continue

def stop()
  return CXChildVisitResult CXChildVisit_Break

struct Parameter
  name: string
  typ: string
  pureType: string
  value: string
  templateRef: string

  queryId: int = -1

  isRW: bool = false

def remove_modifiers(s)
  var r = replace(s, "const", "")
  r = replace(r, "&", "")
  r = replace(r, " ", "")
  return r

def read_struct_fields(structCursor) : array<Parameter>
  var fields: array<Parameter>
  clang_visitChildren(structCursor) <| $(cursor, parent)
    let kind = clang_getCursorKind(cursor)

    if kind == CXCursorKind CXCursor_FieldDecl || kind == CXCursorKind CXCursor_VarDecl
      let typ = clang_getTypeSpelling(clang_getCursorType(cursor))
      fields |> push([[Parameter name = clang_getCursorSpelling(cursor),
                                 typ = typ,
                                 pureType = remove_modifiers(typ),
                                 isRW = find(typ, "&") != -1 && find(typ, "const ") == -1]])
      // auto &p = fields.push_back();
      // p.name = to_string(clang_getCursorSpelling(cursor));
      // p.type = to_string(clang_getTypeSpelling(clang_getCursorType(cursor)));
      // p.pureType = std::regex_replace(p.type.c_str(), std::regex("(?:const|\\&|(?:\\s+))"), "").c_str();

      // p.isRW = p.type.find("&") != eastl::string::npos && p.type.find("const ") == eastl::string::npos;

      // if (kind == CXCursor_VarDecl)
      //   p.value = read_string_literal(cursor);

    return advance()
  return <- fields

[export]
def main()
  let args <- get_command_line_arguments()
  let src = args[1]
  let dst = args[2]

  print("Codegen: {src} -> {dst}")

  let clangArgs : array<string> <- [{string []
    "-xc++";
    "-std=c++14";
    "-w";
    "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH";
    "-D__CODEGEN__";
    "-Wno-invalid-token-paste";
    "-ferror-limit=0";
    "-fsyntax-only";
    "-fdelayed-template-parsing";
    "-fdiagnostics-show-template-tree";
    "-ftemplate-depth=900";
    "-fms-compatibility";
    "-fms-extensions";
    "-fmsc-version=1900";
    "-I../ecs";
    "-I../script";
    "-I../libs/glm";
    "-I../libs/EASTL/include";
    "-I../libs/EASTL/test/packages/EABase/include/Common";
    "-I../libs/raylib/src";
    "-I../libs/Box2D/include";
    "-I../libs/daScript/include"
  }]

  unsafe
    let index = clang_createIndex(0, /* verbose */0)

    let flags = (uint(CXTranslationUnit_Flags CXTranslationUnit_None) |
                 uint(CXTranslationUnit_Flags CXTranslationUnit_DetailedPreprocessingRecord))
    let unit = clang_parseTranslationUnit(index, src, clangArgs, flags)
    if unit.isNull
      print("Unable to parse translation unit: {src}")
      return

    let unitCursor = clang_getTranslationUnitCursor(unit)
    clang_visitChildren(unitCursor) <| $(cursor, parent)
      let kind       = clang_getCursorKind(cursor)
      let parentKind = clang_getCursorKind(parent)

      let spelling     = clang_getCursorSpelling(cursor)
      let kindSpelling = clang_getCursorKindSpelling(kind)

      if (kind == CXCursorKind CXCursor_Namespace ||
          kind == CXCursorKind CXCursor_NamespaceAlias ||
          kind == CXCursorKind CXCursor_NamespaceRef)
        return CXChildVisitResult CXChildVisit_Recurse

      if kind == CXCursorKind CXCursor_StructDecl || kind == CXCursorKind CXCursor_ClassDecl
        if clang_getFileName(cursor) == src
          if starts_with(spelling, "ecs_component_")
            let fields <- read_struct_fields(cursor)
            print("{spelling} = {fields}")

      return advance()

    clang_disposeTranslationUnit(unit)
    clang_disposeIndex(index)