#include <ecs/ecs.h>

#include "update.ecs.h"

#include <EASTL/shared_ptr.h>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <raylib.h>

#include <Box2D/Box2D.h>

struct TestAQuery
{
  ECS_QUERY;

  QL_HAVE(wall, some, etc);
  QL_NOT_HAVE(comp_a, comp_b, comp_c);

  QL_WHERE(a > 1.0 && (b == true || c == false));

  const EntityId &eid;
  const glm::vec4 &collision_rect;
  const glm::vec2 &pos;
};

struct TestBQuery
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

struct UpdatePositionSimple
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

struct UpdatePositionExternalQuery
{
  ECS_RUN(const UpdateStage &stage, AliveEnemy &&enemy)
  {
  }
};

struct UpdatePositionJoin
{
  QL_JOIN(test_a.eid == test_b.eid_a);

  ECS_RUN(const UpdateStage &stage, TestAQuery &&test_a, TestBQuery &&test_b)
  {
  }
};

struct UpdatePositionJoinAll
{
  ECS_RUN(const UpdateStage &stage, TestAQuery &&test_a, TestBQuery &&test_b)
  {
  }
};