// ecs.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>

#include "ecs.h"

struct PositionComponent
{
  float x = 1.f;
  float y = 2.f;
};
REG_COMP(PositionComponent, position);

struct VelocityComponent
{
  float x = 3.f;
  float y = 4.f;
};
REG_COMP(VelocityComponent, velocity);

struct TestComponent
{
  double a = -10.0;
  int b = 1024;
};
REG_COMP(TestComponent, test);

void update_position(const UpdateStage &stage, EntityId eid, const VelocityComponent &velocity, PositionComponent &position)
{
  std::cout << "update_position(" << eid.handle << ")" << std::endl;
  position.x += velocity.x;
  position.y += velocity.y;
}
REG_SYS(update_position);

void position_printer(const UpdateStage &stage, EntityId eid, const PositionComponent &position)
{
  std::cout << "position_printer(" << eid.handle << ")" << std::endl;
  std::cout << "(" << position.x << ", " << position.y << ")" << std::endl;
}
REG_SYS(position_printer);

void test_printer(const UpdateStage &stage, EntityId eid, const TestComponent &test)
{
  std::cout << "test_printer(" << eid.handle << ")" << std::endl;
  std::cout << "TEST (" << test.a << ", " << test.b << ")" << std::endl;
}
REG_SYS(test_printer);

int main()
{
  EntityManager mgr;
  EntityId eid1 = mgr.createEntity("test");
  EntityId eid2 = mgr.createEntity("test");
  EntityId eid3 = mgr.createEntity("test1");

  mgr.tick(UpdateStage{ 0.5f });
  mgr.tick();

  return 0;
}

