// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define NOMINMAX

#include "targetver.h"

#include <assert.h> 

#include <stdio.h>
#include <tchar.h>

#include <vector>

#include "raylib.h"

#pragma warning(push)
#pragma warning(disable: 4244)

#include "raymath.h"

#pragma warning(pop) 

namespace std
{
  using bitarray = vector<bool>;
}

// TODO: reference additional headers your program requires here
