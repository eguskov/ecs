#pragma once

#include "ecs/component.h"

struct TestComponent
{
  double a = -10.0;
  int b = 1024;
  bool set(const JValue &value)
  {
    return true;
  }
};
REG_COMP(TestComponent, test);