#include "daScript/daScript.h"
#include "daScript/simulate/fs_file_info.h"

using namespace das;

void require_project_specific_modules();//link time resolved dependencies
das::FileAccessPtr get_file_access( char * pak );//link time resolved dependencies

#if !defined(DAS_GLOBAL_NEW) && defined(_MSC_VER) && !defined(_WIN64)

void * operator new(std::size_t n) throw(std::bad_alloc)
{
    return das_aligned_alloc16(n);
}
void operator delete(void * p) throw()
{
    das_aligned_free16(p);
}

#endif

static bool quiet = false;
static bool json = false;

static int cursor_x = 0;
static int cursor_y = 0;
static bool cursor = false;

TextPrinter tout;

bool saveToFile ( const string & fname, const string & str ) {
    if ( !quiet )  {
        tout << "saving to " << fname << "\n";
    }
    FILE * f = fopen ( fname.c_str(), "w" );
    if ( !f ) {
        tout << "can't open " << fname << "\n";
        return false;
    }
    fwrite ( str.c_str(), str.length(), 1, f );
    return true;
}

bool compile ( const string & fn, const string & cppFn ) {
    auto access = get_file_access(nullptr);
    ModuleGroup dummyGroup;
    bool firstError = true;
    CodeOfPolicies policies;
    policies.no_optimizations = cursor;
    if ( auto program = compileDaScript(fn,access,tout,dummyGroup,cursor,policies) ) {
        if ( cursor ) {
            auto fi = access->getFileInfo(fn);
            auto cinfo = program->cursor(LineInfo(fi,cursor_x,cursor_y,cursor_x,cursor_y));
            tout << cinfo.reportJson();
            return true;
        }
        if ( program->failed() ) {
            if (json)
                tout << "{ \"result\": \"failed to compile\",\n\"diagnostics\": [\n";
            else
                tout << "failed to compile\n";
            for ( auto & err : program->errors ) {
                if (json) {
                    if (!firstError)
                        tout << ",\n";
                    firstError = false;
                    tout << reportErrorJson(err.at, err.what, err.extra, err.fixme, err.cerr);
                } else {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
                }
            }
            if (json)
                tout << "]\n}";
            return false;
        } else {
            Context ctx(program->getContextStackSize());
            if ( !program->simulate(ctx, tout) ) {
                if (json)
                    tout << "{ \"result\": \"failed to simulate\",\n\"diagnostics\": [\n";
                else
                    tout << "failed to simulate\n";
                for ( auto & err : program->errors ) {
                    if (json) {
                        if (!firstError)
                            tout << ",\n";
                        firstError = false;
                        tout << reportErrorJson(err.at, err.what, err.extra, err.fixme, err.cerr);
                    } else {
                        tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
                    }
                }
                if (json)
                    tout << "]\n}";
                return false;
            }
            // AOT time
            TextWriter tw;
            bool noAotOption = program->options.getBoolOption("no_aot",false);
            bool noAotModule = false;
            // header
            tw << "#include \"daScript/misc/platform.h\"\n\n";

            tw << "#include \"daScript/simulate/simulate.h\"\n";
            tw << "#include \"daScript/simulate/aot.h\"\n";
            tw << "#include \"daScript/simulate/aot_library.h\"\n";
            tw << "\n";
            // lets comment on required modules
            program->library.foreach([&](Module * mod){
                if ( mod->name=="" ) {
                    // nothing, its main program module. i.e ::
                } else {
                    if ( mod->name=="$" ) {
                        tw << " // require builtin\n";
                    } else {
                        tw << " // require " << mod->name << "\n";
                    }
                    if ( mod->aotRequire(tw)==ModuleAotType::no_aot ) {
                        tw << "  // AOT disabled due to this module\n";
                        noAotModule = true;
                    }
                }
                return true;
            },"*");
            if (noAotOption) {
                TextWriter noTw;
                if (!noAotModule)
                  noTw << "// AOT disabled due to options no_aot=true. There are no modules which require no_aot\n\n";
                else
                  noTw << "// AOT disabled due to options no_aot=true. There are also some modules which require no_aot\n\n";
                return saveToFile(cppFn, noTw.str());
            } else if ( noAotModule ) {
                TextWriter noTw;
                noTw << "// AOT disabled due to module requirements\n";
                noTw << "#if 0\n\n";
                noTw << tw.str();
                noTw << "\n#endif\n";
                return saveToFile(cppFn, noTw.str());
            } else {
                tw << "\n";
                tw << "#if defined(_MSC_VER)\n";
                tw << "#pragma warning(push)\n";
                tw << "#pragma warning(disable:4100)   // unreferenced formal parameter\n";
                tw << "#pragma warning(disable:4189)   // local variable is initialized but not referenced\n";
                tw << "#pragma warning(disable:4244)   // conversion from 'int32_t' to 'float', possible loss of data\n";
                tw << "#pragma warning(disable:4114)   // same qualifier more than once\n";
                tw << "#pragma warning(disable:4623)   // default constructor was implicitly defined as deleted\n";
                tw << "#endif\n";
                tw << "#if defined(__GNUC__) && !defined(__clang__)\n";
                tw << "#pragma GCC diagnostic push\n";
                tw << "#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wunused-variable\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wunused-function\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wwrite-strings\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wreturn-local-addr\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wignored-qualifiers\"\n";
                tw << "#pragma GCC diagnostic ignored \"-Wsign-compare\"\n";
                tw << "#endif\n";
                tw << "#if defined(__clang__)\n";
                tw << "#pragma clang diagnostic push\n";
                tw << "#pragma clang diagnostic ignored \"-Wunused-parameter\"\n";
                tw << "#pragma clang diagnostic ignored \"-Wwritable-strings\"\n";
                tw << "#pragma clang diagnostic ignored \"-Wunused-variable\"\n";
                tw << "#pragma clang diagnostic ignored \"-Wunsequenced\"\n";
                tw << "#pragma clang diagnostic ignored \"-Wunused-function\"\n";
                tw << "#endif\n";
                tw << "\n";
                tw << "namespace das {\n";
                tw << "namespace {\n"; // anonymous
                program->aotCpp(ctx, tw);
                // list STUFF
                tw << "struct AotList_impl : AotListBase {\n";
                tw << "\tvirtual void registerAotFunctions ( AotLibrary & aotLib ) override {\n";
                program->registerAotCpp(tw, ctx, false);
                tw << "\t};\n";
                tw << "};\n";
                tw << "AotList_impl impl;\n";
                tw << "}\n";
                tw << "}\n";
                tw << "#if defined(_MSC_VER)\n";
                tw << "#pragma warning(pop)\n";
                tw << "#endif\n";
                tw << "#if defined(__GNUC__) && !defined(__clang__)\n";
                tw << "#pragma GCC diagnostic pop\n";
                tw << "#endif\n";
                tw << "#if defined(__clang__)\n";
                tw << "#pragma clang diagnostic pop\n";
                tw << "#endif\n";
                return saveToFile(cppFn, tw.str());
            }
        }
    } else {
        tout << "failed to compile\n";
        return false;
    }
}

#ifndef MAIN_FUNC_NAME
  #define MAIN_FUNC_NAME main
#endif

int MAIN_FUNC_NAME(int argc, char * argv[]) {
    setCommandLineArguments(argc, argv);
    #ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    #endif
    if ( argc<3 ) {
        tout << "dasAot <in_script.das> <out_script.das.cpp> [-q] [-j]\n";
        return -1;
    }
    if ( argc>3  ) {
        bool scriptArgs = false;
        for (int ai = 3; ai != argc; ++ai) {
            if ( strcmp(argv[ai],"-q")==0 ) {
                quiet = true;
            } else if ( strcmp(argv[ai],"-j")==0 ) {
                json = true;
            } else if ( strcmp(argv[ai],"-cursor")==0 ) {
                if ( ai+2 < argc ) {
                    cursor = true;
                    cursor_x = atoi(argv[ai+1]);
                    cursor_y = atoi(argv[ai+2]);
                    ai += 2;
                } else {
                    tout << "-cursor requires X and Y\n";
                    return -1;
                }
            } else if ( strcmp(argv[ai],"--")==0 ) {
                scriptArgs = true;
            } else if ( !scriptArgs ) {
                tout << "unsupported option " << argv[ai];
                return -1;
            }
        }
    }
    NEED_MODULE(Module_BuiltIn);
    NEED_MODULE(Module_Math);
    NEED_MODULE(Module_Strings);
    NEED_MODULE(Module_Rtti);
    NEED_MODULE(Module_Ast);
    NEED_MODULE(Module_Debugger);
    NEED_MODULE(Module_Random);
    NEED_MODULE(Module_Network);
    NEED_MODULE(Module_UriParser);
    require_project_specific_modules();
    bool compiled = compile(argv[1], argv[2]);
    Module::Shutdown();
    return compiled ? 0 : -1;
}


