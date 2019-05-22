#include <ecs/ecs.h>

#include "update.h"

#include <EASTL/shared_ptr.h>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <raylib.h>

#include <Box2D/Box2D.h>

namespace test
{

struct TestComp
{
  int killCount = 0;

  DEF_SET;
};

ECS_COMPONENT_TYPE(TestComp);

struct EventOnTest : Event
{
  int count;
  EventOnTest(int _count) : count(_count) {}
}
DEF_EVENT(EventOnTest);

}

struct TestA
{
  ECS_QUERY;

  QL_HAVE(wall, some, etc);
  QL_NOT_HAVE(comp_a, comp_b, comp_c);

  QL_WHERE(a > 1.0 && (b == true || c == false));

  const EntityId &eid;
  const glm::vec4 &collision_rect;
  const glm::vec2 &pos;
};

struct TestB
{
  ECS_QUERY;

  QL_HAVE(some);

  const EntityId &eid_a;
  const glm::vec4 &a;
  const glm::vec2 &b;
};

struct BricksQuery
{
  ECS_QUERY;

  QL_HAVE(wall);

  const glm::vec4 &collision_rect;
  const glm::vec2 &pos;
};

struct AliveEnemy
{
  ECS_QUERY;

  QL_HAVE(enemy);
  QL_WHERE(is_alive == true);

  const EntityId &eid;
  const glm::vec4 &collision_rect;
  const glm::vec2 &pos;
  glm::vec2 &vel;
  bool &is_alive;
};

struct Simple
{
  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
  {
  }
};

struct Where
{
  QL_NOT_HAVE(is_active);
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
  {
    AliveEnemy::foreach([&](AliveEnemy &&enemy)
    {
      const float dist = glm::length(pos - enemy.pos);
    });
  }
};

struct ExternalQuery
{
  ECS_RUN(const UpdateStage &stage, AliveEnemy &&enemy)
  {
  }
};

struct ExternalQueryIterable
{
  ECS_RUN_T(const UpdateStage &stage, QueryIterable<AliveEnemy, _> &&enemies)
  {
  }
};

struct JoinAll
{
  ECS_RUN(const UpdateStage &stage, TestA &&test_a, TestB &&test_b)
  {
  }
};

struct JoinIf
{
  QL_JOIN(test_a.eid == test_b.eid_a);

  ECS_RUN(const UpdateStage &stage, TestA &&test_a, TestB &&test_b)
  {
  }
};

struct JoinIndex
{
  QL_INDEX(TestB eid_a);
  QL_INDEX_LOOKUP(test_a.eid);

  ECS_RUN(const UpdateStage &stage, TestA &&test_a, TestB &&test_b)
  {
  }
};

struct GroupBy
{
  ECS_RUN_T(const UpdateStage &stage, int grid_cell, QueryIterable<AliveEnemy, _> &&enemies)
  {
  }
};