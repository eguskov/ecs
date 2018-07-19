#pragma once

#include "stdafx.h"

struct EntityId
{
  uint32_t handle = 0;
  EntityId(uint32_t h = 0) : handle(h) {}
};