#pragma once

#include "stacktrace.h"

#ifdef _DEBUG
// #include <assert.h>
bool assert_handler(const char *message, const char *file, int line);
void fatal_handler(const char *message);
#define ASSERT(expr) (!!(expr)) || (assert_handler(#expr, __FILE__, __LINE__)) || (__debugbreak(), 0)
#define ASSERT_FMT(expr, fmt, ...) (!!(expr)) || (assert_handler(eastl::string(eastl::string::CtorSprintf(), fmt, __VA_ARGS__).c_str(), __FILE__, __LINE__)) || (__debugbreak(), 0)
#define DEBUG_LOG(code) std::cout << code << std::endl;
#else
#define ASSERT(...)
#define ASSERT_FMT(...)
#define DEBUG_LOG(...)
#endif