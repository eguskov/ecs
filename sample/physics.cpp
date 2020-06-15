#include <ecs/ecs.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <raylib.h>

#include <Box2D/Box2D.h>

#include "update.h"
#include "grid.h"

extern int screen_width;
extern int screen_height;

static b2World *g_world = nullptr;

static inline Color to_color(const b2Color& c)
{
  return { (uint8_t)(255.f * c.r), (uint8_t)(255.f * c.g), (uint8_t)(255.f * c.b), (uint8_t)(255.f * c.a) };
}

struct PhysDebugDraw : public b2Draw
{
  /// Draw a closed polygon provided in CCW order.
  void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override final
  {
    DrawSegment(vertices[vertexCount - 1], vertices[0], color);
    for (int i = 0; i < vertexCount - 1; ++i)
      DrawSegment(vertices[i], vertices[i + 1], color);
  }

  /// Draw a solid closed polygon provided in CCW order.
  void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override final
  {
    DrawSegment(vertices[vertexCount - 1], vertices[0], color);
    for (int i = 0; i < vertexCount - 1; ++i)
      DrawSegment(vertices[i], vertices[i + 1], color);
  }

  /// Draw a circle.
  void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override final
  {
    ::DrawCircleV({center.x, center.y}, radius, to_color(color));
  }

  /// Draw a solid circle.
  void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override final
  {
    ::DrawCircleV({center.x, center.y}, radius, to_color(color));
  }

  /// Draw a line segment.
  void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override final
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    ::DrawLineEx({ hw + p1.x, hh + p1.y }, { hw + p2.x, hh + p2.y }, 1.0f, to_color(color));
  }

  /// Draw a transform. Choose your own length scale.
  /// @param xf a transform.
  void DrawTransform(const b2Transform& xf) override final
  {
  }

  /// Draw a point.
  void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override final
  {
    ::DrawCircleV({p.x, p.y}, 4.f, to_color(color));
  }
};

struct PhysicsWorld
{
  ECS_BIND_TYPE(phys, isLocal=true);

  int atTick = 0;

  PhysDebugDraw debugDraw;

  // void operator=(const PhysicsWorld&) { ASSERT(false); }
  ECS_DEFAULT_CTORS(PhysicsWorld);

  // TODO: Use event on destruction
  // ~PhysicsWorld()
  // {
  //   delete g_world;
  //   g_world = nullptr;
  // }
};

ECS_COMPONENT_TYPE(PhysicsWorld);

struct CollisionShape
{
  ECS_BIND_TYPE(phys, canNew=true; isRefType=true; isPod=false; isLocal=true; canCopy=true; canMove=true);

  ECS_BIND(sideEffect=modifyArgument)
  static void add_shape(CollisionShape *&self, const char *type, das::float2 size, das::float2 center, float angle, float density, float friction)
  {
    auto &s = self->shapes.emplace_back();
    s.shape.SetAsBox(0.5f * size.x, 0.5f * size.y, { center.x, center.y }, angle);
    s.density = density;
    s.friction = friction;
  }

  ECS_BIND(sideEffect=modifyArgument)
  static void add_sensor(CollisionShape *&self, const char *type, das::float2 size, das::float2 center)
  {
    auto &s = self->shapes.emplace_back();
    s.shape.SetAsBox(0.5f * size.x, 0.5f * size.y, { center.x, center.y }, 0.f);
    s.density = 0.f;
    s.friction = 0.f;
    s.isSensor = true;
  }

  ECS_DEFAULT_CTORS(CollisionShape);

  struct Shape
  {
    b2PolygonShape shape;

    bool isSensor = false;

    float density = 0.f;
    float friction = 0.f;

    b2Fixture *fixture = nullptr;
  };

  eastl::vector<Shape> shapes;
};

ECS_COMPONENT_TYPE_BIND(CollisionShape);

struct PhysicsBody
{
  ECS_BIND_TYPE(phys, isLocal=true; canCopy=true);

  b2BodyType type;
  b2Body *body;

  ECS_BIND(sideEffect=none; simNode=das::SimNode_ExtFuncCallAndCopyOrMove)
  static PhysicsBody create_phys_body(const char *type)
  {
    b2BodyType bodyType;
    if (::strcmp(type, "static") == 0)
      bodyType = b2_staticBody;
    else if (::strcmp(type, "kinematic") == 0)
      bodyType = b2_kinematicBody;
    else if (::strcmp(type, "dynamic") == 0)
      bodyType = b2_dynamicBody;
    else
      ASSERT(false);

    return {bodyType, nullptr};
  }

  // ECS_DEFAULT_CTORS(PhysicsBody);

  // TODO: Use event instead
  // ~PhysicsBody()
  // {
  //   if (body && g_world)
  //     g_world->DestroyBody(body);
  // }
};

ECS_COMPONENT_TYPE_BIND(PhysicsBody);

struct Brick
{
  ECS_QUERY;

  QL_HAVE(wall);

  QL_INDEX(grid_cell);

  const CollisionShape &collision_shape;
  const glm::vec2 &pos;
};

struct MovingBrick
{
  ECS_QUERY;

  QL_HAVE(wall, auto_move);

  const CollisionShape &collision_shape;
  const glm::vec2 &pos;
  const glm::vec2 &vel;
};

struct AliveEnemy
{
  ECS_QUERY;

  QL_HAVE(enemy);
  QL_WHERE(is_alive == true);

  const EntityId &eid;
  const CollisionShape &collision_shape;
  const glm::vec2 &pos;
  bool &is_alive;
  glm::vec2 &vel;
};

struct PlayerCollision
{
  ECS_QUERY;

  QL_HAVE(user_input);
  QL_WHERE(grid_cell != -1);

  const EntityId &eid;
  const CollisionShape &collision_shape;
  const Gravity &gravity;
  int grid_cell;

  Jump &jump;

  glm::vec2 &pos;
  glm::vec2 &vel;

  bool &is_on_ground;
};

struct EnemyCollision
{
  ECS_QUERY;

  QL_HAVE(enemy, auto_move, wall_collidable);
  QL_WHERE(is_alive == true);

  const EntityId &eid;
  const CollisionShape &collision_shape;

  glm::vec2 &pos;
  glm::vec2 &vel;

  float &dir;

  bool &is_on_ground;
};

struct init_physics_collision_handler
{
  ECS_AFTER(init_physics_world, init_physics_body_handler);

  ECS_RUN(const EventOnEntityCreate &ev, const EntityId &eid, PhysicsBody &phys_body, CollisionShape &collision_shape)
  {
    ASSERT(phys_body.body != nullptr);

    for (auto s : collision_shape.shapes)
    {
      ASSERT(s.shape.Validate());

      b2FixtureDef def;
      def.shape = &s.shape;
      def.density = s.density;
      def.friction = s.friction;
      def.isSensor = s.isSensor;
      def.userData = (void*)eid.handle;
      s.fixture = phys_body.body->CreateFixture(&def);
    }
  }
};

struct init_physics_body_handler
{
  ECS_AFTER(init_physics_world);

  ECS_RUN(const EventOnEntityCreate &ev, PhysicsBody &phys_body, const glm::vec2 &pos)
  {
    ASSERT(phys_body.body == nullptr);

    b2BodyDef def;
    def.type = phys_body.type;
    def.position.Set(pos.x, pos.y);

    phys_body.body = g_world->CreateBody(&def);
  }
};

struct delete_physics_body_handler
{
  ECS_BEFORE(delete_physics_world);

  ECS_RUN(const EventOnEntityDelete &ev, PhysicsBody &phys_body)
  {
    if (phys_body.body && g_world)
      g_world->DestroyBody(phys_body.body);
  }
};

struct init_physics_world
{
  ECS_RUN(const EventOnEntityCreate &ev, PhysicsWorld &phys_world)
  {
    ASSERT(g_world == nullptr);

    g_world = new b2World({ 0.f, -9.81f });
    g_world->SetDebugDraw(&phys_world.debugDraw);
    phys_world.debugDraw.AppendFlags(b2Draw::e_shapeBit);
  }
};

struct delete_physics_world
{
  ECS_RUN(const EventOnEntityDelete &ev, PhysicsWorld &phys_world)
  {
    delete g_world;
    g_world = nullptr;
  }
};

struct tick_physics_world
{
  ECS_AFTER(before_phys_update);
  ECS_BEFORE(after_phys_update);

  // TODO: Add stages for fixed dt updates
  ECS_RUN(const UpdateStage &stage, PhysicsWorld &phys_world)
  {
    const float physDt = 1.f / 60.f;
    const int curTick = (int)ceil(stage.total / (double)physDt);

    if (curTick > phys_world.atTick)
    {
      g_world->Step(physDt, 3, 2);
      phys_world.atTick = curTick;
    }
  }
};

struct render_debug_physics
{
  ECS_AFTER(after_render);

  ECS_RUN(const RenderDebugStage &stage, const PhysicsWorld &phys_world)
  {
    g_world->DebugDraw();
  }
};

struct copy_kinematic_body_state_to_physics
{
  ECS_AFTER(tick_physics_world);

  ECS_RUN(const UpdateStage &stage, PhysicsBody &phys_body, const glm::vec2 &pos, const glm::vec2 &vel)
  {
    ASSERT(phys_body.type != b2_staticBody);

    phys_body.body->SetTransform({ pos.x, pos.y }, 0.f);
    phys_body.body->SetLinearVelocity({ vel.x, vel.y });
  }
};

struct update_player_collisions
{
  ECS_AFTER(collisions_update);

  static __forceinline void process_collision(PlayerCollision &player, Brick &&brick)
  {
    const float minD = player.collision_shape.shapes[0].shape.m_radius + brick.collision_shape.shapes[0].shape.m_radius;

    b2Transform xfA = { { brick.pos.x, brick.pos.y }, b2Rot{0.f} };
    b2Transform xfB = { { player.pos.x, player.pos.y }, b2Rot{0.f} };

    b2Manifold manifold;
    b2CollidePolygons(&manifold, &brick.collision_shape.shapes[0].shape, xfA, &player.collision_shape.shapes[0].shape, xfB);

    int pointIndex = -1;
    for (int i = 0; i < manifold.pointCount && pointIndex < 0; ++i)
    {
      b2WorldManifold worldManifold;
      worldManifold.Initialize(&manifold, xfA, brick.collision_shape.shapes[0].shape.m_radius, xfB, player.collision_shape.shapes[0].shape.m_radius);

      const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
      const glm::vec2 hitPos = { worldManifold.points[i].x, worldManifold.points[i].y };

      const float dy = player.collision_shape.shapes[0].shape.m_centroid.y - 0.5f * (hitPos.y - player.pos.y);
      const float d = -worldManifold.separations[i];

      if (normal.x != 0.f && dy <= minD)
        continue;

      pointIndex = i;
    }

    if (pointIndex >= 0)
    {
      b2WorldManifold worldManifold;
      worldManifold.Initialize(&manifold, xfA, player.collision_shape.shapes[0].shape.m_radius, xfB, brick.collision_shape.shapes[0].shape.m_radius);

      const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
      const glm::vec2 hitPos = { worldManifold.points[pointIndex].x, worldManifold.points[pointIndex].y };

      if (normal.y < 0.f)
        player.is_on_ground = true;

      const float d = -worldManifold.separations[pointIndex];
      const float proj = glm::dot(player.vel, normal);
      player.vel += glm::max(-proj, 0.f) * normal;
      player.pos += glm::max(d - minD, 0.f) * normal;
    }
  }

  ECS_RUN(const UpdateStage &stage, PlayerCollision &&player)
  {
    AliveEnemy::foreach([&](AliveEnemy &&enemy)
    {
      b2Transform xfA = { { enemy.pos.x, enemy.pos.y }, b2Rot{0.f} };
      b2Transform xfB = { { player.pos.x, player.pos.y }, b2Rot{0.f} };

      b2Manifold manifold;
      b2CollidePolygons(&manifold, &enemy.collision_shape.shapes[0].shape, xfA, &player.collision_shape.shapes[0].shape, xfB);

      int pointIndex = -1;
      for (int i = 0; i < manifold.pointCount && pointIndex < 0; ++i)
      {
        b2WorldManifold worldManifold;
        worldManifold.Initialize(&manifold, xfA, enemy.collision_shape.shapes[0].shape.m_radius, xfB, player.collision_shape.shapes[0].shape.m_radius);

        const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
        const glm::vec2 hitPos = { worldManifold.points[i].x, worldManifold.points[i].y };

        const float dy = player.collision_shape.shapes[0].shape.m_centroid.y - 0.5f * (hitPos.y - player.pos.y);
        const float d = -worldManifold.separations[i];

        if (normal.y >= 0.f)
          continue;

        pointIndex = i;
      }

      if (pointIndex >= 0)
      {
        ecs::delete_entity(enemy.eid);
        ecs::send_event(player.eid, EventOnKillEnemy{ enemy.pos });

        enemy.is_alive = false;
        enemy.vel = glm::vec2(0.f, 0.f);

        player.jump.active = true;
        player.jump.startTime = stage.total;
      }
    });

    MovingBrick::foreach([&](MovingBrick &&brick)
    {
      b2Transform xfA = { { brick.pos.x, brick.pos.y }, b2Rot{0.f} };
      b2Transform xfB = { { player.pos.x, player.pos.y }, b2Rot{0.f} };

      b2Manifold manifold;
      b2CollidePolygons(&manifold, &brick.collision_shape.shapes[1].shape, xfA, &player.collision_shape.shapes[0].shape, xfB);

      if (manifold.pointCount > 0 && glm::dot(player.vel, brick.vel) >= 0.f)
      {
        player.vel += brick.vel;
        // myVel.y = gravity.mass * 9.8f * stage.dt;
      }
    });

    glm::vec2 box(64.f, 64.f);
    const int boxCellLeft = MAKE_GRID_INDEX(player.pos.x - box.x, GRID_CELL_SIZE);
    const int boxCellTop = MAKE_GRID_INDEX(player.pos.y - box.y, GRID_CELL_SIZE);
    const int boxCellRight = MAKE_GRID_INDEX(player.pos.x + box.x, GRID_CELL_SIZE);
    const int boxCellBottom = MAKE_GRID_INDEX(player.pos.y + box.y, GRID_CELL_SIZE);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellTop; y <= boxCellBottom; ++y)
      {
        if (Query *cell = Brick::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
            process_collision(player, Brick::get(q));
      }
  }
};

struct render_debug_player_grid_cell
{
  ECS_AFTER(after_render);

  QL_HAVE(user_input);
  QL_WHERE(grid_cell != -1);

  ECS_RUN(const RenderDebugStage &stage, const glm::vec2 &pos, int grid_cell)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    glm::vec2 box(64.f, 64.f);
    const int boxCellLeft = MAKE_GRID_INDEX(pos.x - box.x, GRID_CELL_SIZE);
    const int boxCellTop = MAKE_GRID_INDEX(pos.y - box.y, GRID_CELL_SIZE);
    const int boxCellRight = MAKE_GRID_INDEX(pos.x + box.x, GRID_CELL_SIZE);
    const int boxCellBottom = MAKE_GRID_INDEX(pos.y + box.y, GRID_CELL_SIZE);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellTop; y <= boxCellBottom; ++y)
      {
        DrawRectangleV(Vector2{hw + float(x) * float(GRID_CELL_SIZE), hh + float(y) * float(GRID_CELL_SIZE)}, Vector2{float(GRID_CELL_SIZE), float(GRID_CELL_SIZE)}, Color{ 0, 117, 44, 28 });

        if (Query *cell = Brick::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
          {
            Brick brick = Brick::get(q);
            DrawCircleV(Vector2{hw + brick.pos.x, hh + brick.pos.y}, 5.f, Color{ 255, 0, 0, 100 });
          }
      }
  }
};

struct update_auto_move_collisions
{
  ECS_AFTER(collisions_update);
  ECS_BEFORE(update_player_collisions);

  static __forceinline void process_collision(EnemyCollision &enemy, Brick &&brick)
  {
    b2Transform xfA = { { brick.pos.x, brick.pos.y }, b2Rot{0.f} };
    b2Transform xfB = { { enemy.pos.x, enemy.pos.y }, b2Rot{0.f} };

    b2Manifold manifold;
    b2CollidePolygons(&manifold, &brick.collision_shape.shapes[0].shape, xfA, &enemy.collision_shape.shapes[0].shape, xfB);

    int pointIndex = -1;
    for (int i = 0; i < manifold.pointCount && pointIndex < 0; ++i)
    {
      pointIndex = i;
    }

    if (pointIndex >= 0)
    {
      b2WorldManifold worldManifold;
      worldManifold.Initialize(&manifold, xfA, enemy.collision_shape.shapes[0].shape.m_radius, xfB, brick.collision_shape.shapes[0].shape.m_radius);

      const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
      const glm::vec2 hitPos = { worldManifold.points[pointIndex].x, worldManifold.points[pointIndex].y };

      const float d = -worldManifold.separations[pointIndex];
      const float proj = glm::dot(enemy.vel, normal);

      if (glm::dot(normal, enemy.vel) < 0.f &&
          (normal.x < 0.f || normal.x > 0.f))
      {
        enemy.vel.x = normal.x * abs(enemy.vel.x);
        enemy.dir = -enemy.dir;
      }

      if (glm::dot(normal, enemy.vel) < 0.f && normal.y < 0.f)
      {
        enemy.is_on_ground = true;
        enemy.vel.x = 0.f;
      }

      if (proj < 0.f)
        enemy.vel += fabsf(proj) * normal;
      enemy.pos += fabsf(d) * normal;
    }
  }

  ECS_RUN(const UpdateStage &stage, EnemyCollision &&enemy)
  {
    glm::vec2 box(64.f, 64.f);
    const int boxCellLeft = MAKE_GRID_INDEX(enemy.pos.x - box.x, GRID_CELL_SIZE);
    const int boxCellTop = MAKE_GRID_INDEX(enemy.pos.y - box.y, GRID_CELL_SIZE);
    const int boxCellRight = MAKE_GRID_INDEX(enemy.pos.x + box.x, GRID_CELL_SIZE);
    const int boxCellBottom = MAKE_GRID_INDEX(enemy.pos.y + box.y, GRID_CELL_SIZE);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellTop; y <= boxCellBottom; ++y)
      {
        if (Query *cell = Brick::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
            process_collision(enemy, Brick::get(q));
      }
  }
};