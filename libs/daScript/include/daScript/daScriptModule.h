#pragma once
#include <stdint.h>

namespace das
{
    extern unsigned ModuleKarma;
    class Module;
};

#define NEED_MODULE(ClassName) \
    extern das::Module * register_##ClassName (); \
    das::ModuleKarma += unsigned(intptr_t(register_##ClassName()));

#define NEED_ALL_DEFAULT_MODULES \
    NEED_MODULE(Module_BuiltIn); \
    NEED_MODULE(Module_Math); \
    NEED_MODULE(Module_Strings); \
    NEED_MODULE(Module_Rtti); \
    NEED_MODULE(Module_Ast); \
    NEED_MODULE(Module_Debugger); \
    NEED_MODULE(Module_FIO); \
    NEED_MODULE(Module_Random); \
    NEED_MODULE(Module_Network);

