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

// TODO: CODEGEN
// static void UpdatePositionJoinIndex_run(const RawArg &stage_or_event, Query&)
// {
//   Index &index = *g_mgr->getIndexByName(HASH("codegen-test.cpp_UpdatePositionJoinIndex_eid_a"));
//   Query &query1 = *g_mgr->getQueryByName(HASH("codegen-test.cpp_TestAQuery"));
//   Query &query2 = *g_mgr->getQueryByName(HASH("codegen-test.cpp_TestBQuery"));
//   for (auto q1 = query1.begin(), e = query1.end(); q1 != e; ++q1)
//   {
//     TestAQuery test_a =
//     {
//       GET_COMPONENT(TestAQuery, q1, EntityId, eid),
//       GET_COMPONENT(TestAQuery, q1, glm::vec4, collision_rect),
//       GET_COMPONENT(TestAQuery, q1, glm::vec2, pos)
//     };

//     if (auto q2 = index.find(test_a.eid))
//     {
//       TestBQuery test_b =
//       {
//         GET_COMPONENT(TestBQuery, (*q2), EntityId, eid_a),
//         GET_COMPONENT(TestBQuery, (*q2), glm::vec4, a),
//         GET_COMPONENT(TestBQuery, (*q2), glm::vec2, b)
//       };
//       UpdatePositionJoinIndex::run(*(UpdateStage*)stage_or_event.mem, eastl::move(test_a), eastl::move(test_b));
//     }
//   }
// }

struct UpdatePositionJoinIndex
{
  QL_MAKE_INDEX(eid_a);

  QL_JOIN(test_a.eid == test_b.eid_a);

  ECS_RUN(const UpdateStage &stage, TestAQuery &&test_a, TestBQuery &&test_b)
  {
  }
};

struct UpdatePositionGroupBy
{
  template <typename Builder>
  static void run(const UpdateStage &stage, /* This is filed for Group By and Index */ int grid_cell, QueryIterable<AliveEnemy, Builder> &&enemies)
  {
  }
};

// TODO: Detect QueryIterable and do proper code
// UpdatePositionExternalQuery::run(stage, QueryIterable<AliveEnemy, AliveEnemyBuilder>(query))
// static void UpdatePositionExternalQueryIterabley_run(const RawArg &stage_or_event, Query&)
// {
//   using AliveEnemyBuilder = StructBuilder<
//     StructField<int, INDEX_OF_COMPONENT(AliveEnemy, grid_cell)>,
//     StructField<float, INDEX_OF_COMPONENT(AliveEnemy, float_comp)>
//   >;
//   Query &query = *g_mgr->getQueryByName(HASH("codegen-test.cpp_AliveEnemy"));
//   UpdatePositionExternalQueryIterable::run(*(UpdateStage*)stage_or_event.mem, QueryIterable<AliveEnemy, AliveEnemyBuilder>(query));
// }
struct UpdatePositionExternalQueryIterable
{
  template <typename Builder>
  static void run(const UpdateStage &stage, QueryIterable<AliveEnemy, Builder> &&enemies)
  {
  }
};