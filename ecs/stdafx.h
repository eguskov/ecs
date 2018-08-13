// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define NOMINMAX

#include "targetver.h"

#include <new>

void* operator new(std::size_t s);
void* operator new[](std::size_t s);
void operator delete(void*p);
void operator delete[](void*p);
void operator delete(void *p, size_t);
void operator delete[](void *p, size_t);

#include <assert.h> 

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>

#include <iostream>

#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/set.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/bitset.h>
#include <EASTL/bitvector.h>
#include <EASTL/algorithm.h>
#include <EASTL/queue.h>
#include <EASTL/sort.h>
#include <EASTL/tuple.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <EASTL/string_map.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
