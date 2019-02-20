#include "debug.h"

#include <windows.h>

#include <sstream>

bool assert_handler(const char *message, const char *file, int line)
{
  // TODO: Write to log

  std::ostringstream oss;

  oss << message << "\n\n";

  eastl::string stack;
  stacktrace::print(stack);
  oss << stack.c_str();

  int res = ::MessageBox(NULL, oss.str().c_str(), "Assert", MB_ABORTRETRYIGNORE | MB_ICONERROR);
  if (res == IDABORT)
    exit(-1);
  return res != IDRETRY;
}

void fatal_handler(const char *message)
{
  // TODO: Write to log

  ::MessageBox(NULL, message, "FATAL", MB_OK | MB_ICONERROR);
}