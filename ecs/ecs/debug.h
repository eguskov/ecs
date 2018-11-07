#pragma once

#ifdef _DEBUG
#include <assert.h>
#define ASSERT(...) assert(__VA_ARGS__)
#define DEBUG_LOG(code) std::cout << code << std::endl;
#else
#define ASSERT(...)
#define DEBUG_LOG(...)
#endif