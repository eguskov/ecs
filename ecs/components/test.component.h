#pragma once

#include "ecs/component.h"

struct TestComponent
{
  double a = -10.0;
  int b = 1024;
};
REG_COMP(TestComponent, test);