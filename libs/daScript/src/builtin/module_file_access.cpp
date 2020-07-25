#include "daScript/misc/platform.h"
#include "daScript/ast/ast.h"

namespace das {

    ModuleFileAccess::ModuleFileAccess() {
    }

    ModuleFileAccess::~ModuleFileAccess() {
        if (context) delete context;
    }

    ModuleFileAccess::ModuleFileAccess ( const string & pak, const FileAccessPtr & access ) {
        ModuleGroup dummyLibGroup;
        TextWriter tout;
        if ( auto program = compileDaScript(pak, access, tout, dummyLibGroup) ) {
            if ( program->failed() ) {
                for ( auto & err : program->errors ) {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                }
                DAS_FATAL_LOG("failed to compile: %s\n", pak.c_str());
                DAS_FATAL_LOG("%s", tout.str().c_str());
                DAS_FATAL_ERROR;
            } else {
                context = new Context();
                if ( !program->simulate(*context, tout) ) {
                    tout << "failed to simulate\n";
                    for ( auto & err : program->errors ) {
                        tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                    }
                    delete context;
                    context = nullptr;
                    DAS_FATAL_LOG("failed to simulate: %s\n", pak.c_str());
                    DAS_FATAL_LOG("%s", tout.str().c_str());
                    DAS_FATAL_ERROR;
                    return;
                }
                modGet = context->findFunction("module_get");
                DAS_ASSERTF(modGet!=nullptr, "can't find module_get function");
                // get it ready
                context->restart();
                context->runInitScript();
                // setup pak root
                int ipr = context->findVariable("DAS_PAK_ROOT");
                if ( ipr != -1 ) {
                    // TODO: verify if its string
                    string pakRoot = ".";
                    auto np = pak.find_last_of("\\/");
                    if ( np != string::npos ) {
                        pakRoot = pak.substr(0,np+1);
                    }
                    // set it
                    char ** gpr = (char **) context->getVariable(ipr);
                    *gpr = context->stringHeap->allocateString(pakRoot);
                }
                // logs
                auto tostr = tout.str();
                if ( !tostr.empty() ) {
                    printf("%s\n", tostr.c_str());
                }
            }
        } else {
            DAS_FATAL_LOG("failed to compile: %s\n", pak.c_str());
            DAS_FATAL_LOG("%s", tout.str().c_str());
            DAS_FATAL_ERROR;
        }
    }

    ModuleInfo ModuleFileAccess::getModuleInfo ( const string & req, const string & from ) const {
        if (failed()) return FileAccess::getModuleInfo(req, from);
        vec4f args[2];
        args[0] = cast<const char *>::from(req.c_str());
        args[1] = cast<const char *>::from(from.c_str());
        struct {
            char * modName;
            char * modFileName;
            char * modImportName;
        } res;
        memset(&res, 0, sizeof(res));
        context->evalWithCatch(modGet, args, &res);
        auto exc = context->getException();
        DAS_ASSERTF(!exc, "exception failed: %s", exc);
        ModuleInfo info;
        info.moduleName = res.modName ? res.modName : "";
        info.fileName = res.modFileName ? res.modFileName : "";
        info.importName = res.modImportName ? res.modImportName : "";
        return info;
    }
}
