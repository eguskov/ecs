#include "daScript/misc/platform.h"

#include "daScript/misc/sysos.h"

#if defined(_WIN32)
    #include <windows.h>
    namespace das {
        size_t getExecutablePathName(char* pathName, size_t pathNameCapacity) {
            return GetModuleFileNameA(NULL, pathName, (DWORD)pathNameCapacity);
        }
    }
#elif defined(__linux__)
    #include <unistd.h>
    namespace das {
        size_t getExecutablePathName(char* pathName, size_t pathNameCapacity) {
            size_t pathNameSize = readlink("/proc/self/exe", pathName, pathNameCapacity - 1);
            pathName[pathNameSize] = '\0';
            return pathNameSize;
        }
    }
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
    namespace das {
        size_t getExecutablePathName(char* pathName, size_t pathNameCapacity) {
            uint32_t pathNameSize = 0;
            _NSGetExecutablePath(NULL, &pathNameSize);
            if (pathNameSize > pathNameCapacity)
                pathNameSize = pathNameCapacity;
            if (!_NSGetExecutablePath(pathName, &pathNameSize)) {
                char real[PATH_MAX];
                if (realpath(pathName, real) != NULL) {
                    pathNameSize = strlen(real);
                    memcpy(pathName, real, pathNameSize+1);
                }
                return pathNameSize;
            }
            return 0;
        }
    }
#else
    namespace das {
        size_t getExecutablePathName(char*, size_t) {
            DAS_FATAL_LOG("platforms without getExecutablePathName should not use default getDasRoot");
            DAS_FATAL_ERROR;
            return 0;
        }
    }
#endif

namespace das {
    string getExecutableFileName ( void ) {
        char buffer[1024];
        return getExecutablePathName(buffer,1024) ? buffer : "";
    }

    string get_prefix ( const string & req ) {
        auto np = req.find_last_of("\\/");
        if ( np != string::npos ) {
            return req.substr(0,np);
        } else {
            return req;
        }
    }

    string get_suffix ( const string & req ) {
        auto np = req.find_last_of("\\/");
        if ( np != string::npos ) {
            return req.substr(np+1);
        } else {
            return req;
        }
    }

    string g_dasRoot;

    void setDasRoot ( const string & dr ) {
        g_dasRoot = dr;
    }

    string getDasRoot ( void ) {
        if ( g_dasRoot.empty() ) {
            string efp = getExecutableFileName();   // ?/bin/debug/binary.exe
            auto np = efp.find_last_of("\\/");
            if ( np != string::npos ) {
                auto ep = get_prefix(efp);          // remove file name
                auto suffix = get_suffix(ep);
                if ( suffix != "bin" ) {
                    ep = get_prefix(ep);            // remove debug
                }
                DAS_ASSERT(get_suffix(ep)=="bin");
                g_dasRoot = get_prefix(ep);         // remove bin
            } else {
                g_dasRoot = ".";
            }
        }
        return g_dasRoot;
    }
}
