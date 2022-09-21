#include "stacktrace.h"

#ifdef _DEBUG
#include "debug.h"

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <signal.h>

#include <EASTL/array.h>

#include <sstream>
#include <iostream>
#include <iomanip>

#ifdef max
#undef max
#endif

using Callstack = eastl::array<uintptr_t, 32>;
static constexpr uintptr_t INVALID_ADDR = std::numeric_limits<uintptr_t>::max();

static void read_stack(::CONTEXT &ctx, Callstack &stack)
{
  eastl::fill(stack.begin(), stack.end(), INVALID_ADDR);

  ::STACKFRAME64 frm;
  ::memset(&frm, 0, sizeof(frm));
  frm.AddrPC.Offset    = ctx.Rip;
  frm.AddrPC.Mode      = AddrModeFlat;
  frm.AddrStack.Offset = ctx.Rsp;
  frm.AddrStack.Mode   = AddrModeFlat;
  frm.AddrFrame.Offset = ctx.Rbp;
  frm.AddrFrame.Mode   = AddrModeFlat;

  HANDLE ph = ::GetCurrentProcess();
  HANDLE th = ::GetCurrentThread();

  for (uintptr_t &addr : stack)
  {
    if (!::StackWalk64(IMAGE_FILE_MACHINE_I386, ph, th, &frm, &ctx, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
      break;
    addr = frm.AddrPC.Offset;
  }
}

static void print_stack(const Callstack &stack, std::ostringstream &oss)
{
  HANDLE ph = ::GetCurrentProcess();

  for (size_t i = 0; i < stack.size(); ++i)
  {
    struct Sym : public IMAGEHLP_SYMBOL
    {
      char nm[256];
    };
  
    IMAGEHLP_MODULE mod;
    Sym sym;
    ULONG_PTR ofs;
    unsigned long col=0;
    IMAGEHLP_LINE line;
    bool sym_ok = false, line_ok = false;
    uintptr_t stackAddr = stack[i];

    if (stackAddr == INVALID_ADDR)
      break;

    sym.SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL);
    sym.MaxNameLength = sizeof(sym.nm);
    mod.SizeOfStruct  = sizeof(IMAGEHLP_MODULE);
    line.SizeOfStruct = sizeof(line);

    oss << "#" << i + 1 << ": ";

    if (::SymGetSymFromAddr(ph, stackAddr, &ofs, &sym))
    {
      char name[256];
      if (!::UnDecorateSymbolName(sym.Name, name, _countof(name), UNDNAME_NO_MEMBER_TYPE | UNDNAME_NO_ACCESS_SPECIFIERS | UNDNAME_NO_ALLOCATION_MODEL | UNDNAME_NO_MS_KEYWORDS))
        if (!::SymUnDName(&sym, name, 128))
          ::strcpy_s(name, sym.Name);

      oss << name;

      if (::SymGetLineFromAddr(ph, stackAddr, &col, &line))
        oss << " (" << line.FileName << ", " << line.LineNumber << ", " << col << ")";
    }
    else
      oss << "0x" << std::hex << stackAddr << " ???" << std::dec;

    oss << "\n";
  }
}

static void signal_handler(int sig)
{
  std::ostringstream oss;

  oss << "Signal: ";

  if (sig == SIGABRT) oss << "SIGABRT";
  else if (sig == SIGFPE) oss << "SIGFPE";
  else if (sig == SIGILL) oss << "SIGILL";
  else if (sig == SIGSEGV) oss << "SIGSEGV";

  assert_handler(oss.str().c_str(), __FILE__, __LINE__);

  __debugbreak();
}

static const char* exception_type_to_string(uint32_t code)
{
  switch (code)
  {
    case EXCEPTION_ACCESS_VIOLATION:
      return "access violation";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "misaligned data access";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return "out of bounds access";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return "floating point /0";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return "floating point error";
    case EXCEPTION_FLT_OVERFLOW:
      return "floating point overflow";
    case EXCEPTION_FLT_STACK_CHECK:
      return "floating point stack over/underflow";
    case EXCEPTION_FLT_UNDERFLOW:
      return "floating point underflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "integer /0";
    case EXCEPTION_INT_OVERFLOW:
      return "integer overflow";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "priv instruction";
    case EXCEPTION_IN_PAGE_ERROR:
      return "page is inaccessible";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "illegal opcode";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return "attempt to continue after noncontinuable exception";
    case EXCEPTION_STACK_OVERFLOW:
      return "stack overflow";
    case EXCEPTION_INVALID_DISPOSITION:
      return "invalid disposition";
    case EXCEPTION_INVALID_HANDLE:
      return "invalid handle";
    case STATUS_FLOAT_MULTIPLE_TRAPS:
      return "float trap";
    case STATUS_FLOAT_MULTIPLE_FAULTS:
      return "float faults";
    default:
      return "*unknown type*";
  }
}

static long __stdcall unhandled_exception_filter(EXCEPTION_POINTERS *ep)
{
  switch (ep->ExceptionRecord->ExceptionCode)
  {
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_GUARD_PAGE:
      return EXCEPTION_CONTINUE_SEARCH;

    case EXCEPTION_SINGLE_STEP:
    case EXCEPTION_BREAKPOINT:
      return !::IsDebuggerPresent();
  }

  std::ostringstream oss;
  oss << "Exception!\n\n" << exception_type_to_string(ep->ExceptionRecord->ExceptionCode) << "\n\n";

  Callstack stack;
  read_stack(*ep->ContextRecord, stack);

  print_stack(stack, oss);

  fatal_handler(oss.str().c_str());

  return EXCEPTION_CONTINUE_SEARCH;
}

static bool find_module_addr(const char *pe_img_name, uintptr_t &base_addr, uint32_t &module_size)
{
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
  MODULEENTRY32 me32;

  hModuleSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  if (hModuleSnap == INVALID_HANDLE_VALUE)
    return false;

  me32.dwSize = sizeof(MODULEENTRY32);

  if (!::Module32First(hModuleSnap, &me32))
  {
    ::CloseHandle(hModuleSnap);
    return false;
  }

  do
  {
    if (::strcmp(me32.szExePath, pe_img_name) == 0 || ::strcmp(me32.szModule, pe_img_name) == 0)
    {
      ::CloseHandle(hModuleSnap);
      base_addr = (uintptr_t)me32.modBaseAddr;
      module_size = me32.modBaseSize;
      return true;
    }
  }
  while (::Module32Next(hModuleSnap, &me32));

  ::CloseHandle(hModuleSnap);
  return false;
}

static bool load_module(const char *pe_img_name, uintptr_t base_addr = 0, uint32_t module_size = 0)
{
  if (base_addr == 0)
    find_module_addr(pe_img_name, base_addr, module_size);

  return ::SymLoadModule64(::GetCurrentProcess(), NULL, pe_img_name, NULL, base_addr, module_size);
}

void stacktrace::init()
{
  ::SymInitialize(::GetCurrentProcess(), ".;", false);
  ::SymSetOptions(SYMOPT_DEFERRED_LOADS);

  char exe_path[MAX_PATH];
  ::GetModuleFileName(::GetModuleHandle(NULL), exe_path, _countof(exe_path));
  load_module(exe_path);

  static const char* const knownModules[] = { "ntdll.dll", "kernelbase.dll", "kernel32.dll", "user32.dll" };
  for (int i = 0; i < _countof(knownModules); ++i)
  {
    MODULEINFO mi;
    HMODULE hmod = ::GetModuleHandle(knownModules[i]);
    if (hmod && ::GetModuleInformation(GetCurrentProcess(), hmod, &mi, sizeof(mi)))
      load_module(knownModules[i], (uintptr_t)mi.lpBaseOfDll, mi.SizeOfImage);
  }

  ::signal(SIGABRT, signal_handler);
  // ::signal(SIGFPE, signal_handler);
  // ::signal(SIGILL, signal_handler);
  // ::signal(SIGSEGV, signal_handler);

  ::SetUnhandledExceptionFilter(&unhandled_exception_filter);
}

void stacktrace::release()
{
  ::SymCleanup(::GetCurrentProcess());
}

void stacktrace::print(eastl::string &res)
{
  Callstack stack;

  std::ostringstream oss;

  oss << "Main thread:\n";

  ::CONTEXT ctx;
  ::RtlCaptureContext(&ctx);

  read_stack(ctx, stack);
  print_stack(stack, oss);

  res = oss.str().c_str();
}

void stacktrace::print()
{
  eastl::string s;
  stacktrace::print(s);

  DEBUG_LOG(s.c_str());
}

static void print_thread_callstack(int32_t thread_id)
{
  HANDLE thread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, thread_id);
  if (thread == NULL)
    return;

  ::CONTEXT ctx = { 0 };
  ctx.ContextFlags = CONTEXT_ALL & ~CONTEXT_DEBUG_REGISTERS;
  if (::SuspendThread(thread) == (DWORD)-1)
  {
    ::CloseHandle(thread);
    return;
  }

  bool ctxCaptured = ::GetThreadContext(thread, &ctx) != 0;
  ::ResumeThread(thread);

  if (ctxCaptured)
  {
    Callstack stack;
    read_stack(ctx, stack);

    std::ostringstream oss;
    oss << "Thread " << thread_id << ":\n";
    print_stack(stack, oss);

    DEBUG_LOG(oss.str());
  }

  ::CloseHandle(thread);
}

void stacktrace::print_all_threads()
{
  HANDLE h = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (h != INVALID_HANDLE_VALUE)
  {
    DWORD cpid = ::GetCurrentProcessId();
    DWORD ctid = ::GetCurrentThreadId();

    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (::Thread32First(h, &te))
    {
      do
      {
        if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) && ctid != te.th32ThreadID && cpid == te.th32OwnerProcessID)
          print_thread_callstack(te.th32ThreadID);
        te.dwSize = sizeof(te);
      }
      while (::Thread32Next(h, &te));
    }
    ::CloseHandle(h);
  }
}
#endif