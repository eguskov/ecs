#include <gtest/gtest.h>

#include <ecs/ecs.h>

PULL_ESC_CORE;

int main(int argc, char **argv)
{
  stacktrace::init();

  EntityManager::create();

  testing::InitGoogleTest(&argc, argv);
  int res = RUN_ALL_TESTS();

  EntityManager::release();

  stacktrace::release();

  return res;
}