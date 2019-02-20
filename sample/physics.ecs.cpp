#include <ecs/ecs.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <raylib.h>

#include <Box2D/Box2D.h>

#include "update.ecs.h"

extern Camera2D camera;
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
  void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override final
  {
    ::DrawCircleV({center.x, center.y}, radius, to_color(color));
  }

  /// Draw a solid circle.
  void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override final
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
  void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color) override final
  {
    ::DrawCircleV({p.x, p.y}, 4.f, to_color(color));
  }
};

struct EventOnPhysicsContact : Event
{
  glm::vec2 vel;

  EventOnPhysicsContact() = default;
  EventOnPhysicsContact(const glm::vec2 v) : vel(v) {}
}
DEF_EVENT(EventOnPhysicsContact);

struct PhysicsWorld
{
  int atTick = 0;

  PhysDebugDraw debugDraw;

  void operator=(const PhysicsWorld&) { ASSERT(false); }

  bool set(const JFrameValue &value)
  {
    return true;
  }

  ~PhysicsWorld()
  {
    delete g_world;
    g_world = nullptr;
  }
}
DEF_COMP(PhysicsWorld, phys_world);

struct CollisionShape
{
  struct Shape
  {
    b2PolygonShape shape;

    bool isSensor = false;

    float density = 0.f;
    float friction = 0.f;

    b2Fixture *fixture = nullptr;
  };

  eastl::vector<Shape> shapes;

  CollisionShape(CollisionShape &&assign)
  {
    *this = eastl::move(assign);
  }

  void operator=(CollisionShape &&assign)
  {
    shapes = eastl::move(assign.shapes);
  }

  CollisionShape() = default;
  CollisionShape(const CollisionShape&) { ASSERT(false); }
  void operator=(const CollisionShape&) { ASSERT(false); }

  bool set(const JFrameValue &value)
  {
    for (int i = 0; i < (int)value.Size(); ++i)
    {
      auto &s = shapes.emplace_back();
      s.shape.SetAsBox(
        0.5f * value[i]["size"][0].GetFloat(),
        0.5f * value[i]["size"][1].GetFloat(),
        { value[i]["center"][0].GetFloat(), value[i]["center"][1].GetFloat() },
        value[i]["angle"].GetFloat());

      s.density = value[i]["material"]["density"].GetFloat();
      s.friction = value[i]["material"]["friction"].GetFloat();

      if (value[i].HasMember("isSensor"))
        s.isSensor = value[i]["isSensor"].GetBool();
    }
    return true;
  }
}
DEF_COMP(CollisionShape, collision_shape);

struct PhysicsBody
{
  b2BodyType type = b2_staticBody;
  b2Body *body = nullptr;

  PhysicsBody() = default;
  PhysicsBody(PhysicsBody &&other)
  {
    if (body)
      g_world->DestroyBody(body);
    type = other.type;
    body = other.body;
    other.body = nullptr;
  }

  void operator=(PhysicsBody &&other)
  {
    if (body)
      g_world->DestroyBody(body);
    type = other.type;
    body = other.body;
    other.body = nullptr;
  }

  PhysicsBody(const PhysicsBody&) { ASSERT(false); }
  void operator=(const PhysicsBody&) { ASSERT(false); }

  ~PhysicsBody()
  {
    if (body && g_world)
      g_world->DestroyBody(body);
  }

  bool set(const JFrameValue &value)
  {
    eastl::string t = value["type"].GetString();
    if (t == "static")
      type = b2_staticBody;
    else if (t == "kinematic")
      type = b2_kinematicBody;
    else if (t == "dynamic")
      type = b2_dynamicBody;
    else
      ASSERT(false);

    return true;
  }
}
DEF_COMP(PhysicsBody, phys_body);

struct init_physics_collision_handler
{
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
  ECS_RUN(const EventOnEntityCreate &ev, PhysicsBody &phys_body, const glm::vec2 &pos)
  {
    ASSERT(phys_body.body == nullptr);

    b2BodyDef def;
    def.type = phys_body.type;
    def.position.Set(pos.x, pos.y);

    phys_body.body = g_world->CreateBody(&def);
  }
};

struct init_physics_world_handler
{
  ECS_RUN(const EventOnEntityCreate &ev, PhysicsWorld &phys_world)
  {
    ASSERT(g_world == nullptr);

    g_world = new b2World({ 0.f, -9.81f });
    g_world->SetDebugDraw(&phys_world.debugDraw);
    phys_world.debugDraw.AppendFlags(b2Draw::e_shapeBit);
  }
};

struct update_physics
{
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

struct update_kinematic_physics_body
{
  ECS_RUN( const UpdateStage &stage, PhysicsBody &phys_body, const glm::vec2 &pos, const glm::vec2 &vel)
  {
    ASSERT(phys_body.type != b2_staticBody);

    phys_body.body->SetTransform({ pos.x, pos.y }, 0.f);
    phys_body.body->SetLinearVelocity({ vel.x, vel.y });
  }
};

struct Brick
{
  ECS_QUERY;

  QL_HAVE(wall);

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

struct update_physics_collisions
{
  QL_HAVE(user_input);

  ECS_RUN(const UpdateStage &stage, const PhysicsBody &phys_body, const CollisionShape &collision_shape, const Gravity &gravity, bool &is_on_ground, glm::vec2 &pos, glm::vec2 &vel)
  {
    MovingBrick::foreach([&](MovingBrick &&brick)
    {
      b2Transform xfA = { { brick.pos.x, brick.pos.y }, b2Rot{0.f} };
      b2Transform xfB = { { pos.x, pos.y }, b2Rot{0.f} };

      b2Manifold manifold;
      b2CollidePolygons(&manifold, &brick.collision_shape.shapes[1].shape, xfA, &collision_shape.shapes[0].shape, xfB);

      if (manifold.pointCount > 0 && glm::dot(vel, brick.vel) >= 0.f)
      {
        vel += brick.vel;
        // myVel.y = gravity.mass * 9.8f * stage.dt;
      }
    });

    Brick::foreach([&](Brick &&brick)
    {
      const float minD = collision_shape.shapes[0].shape.m_radius + brick.collision_shape.shapes[0].shape.m_radius;

      b2Transform xfA = { { brick.pos.x, brick.pos.y }, b2Rot{0.f} };
      b2Transform xfB = { { pos.x, pos.y }, b2Rot{0.f} };

      b2Manifold manifold;
      b2CollidePolygons(&manifold, &brick.collision_shape.shapes[0].shape, xfA, &collision_shape.shapes[0].shape, xfB);

      int pointIndex = -1;
      for (int i = 0; i < manifold.pointCount && pointIndex < 0; ++i)
      {
        b2WorldManifold worldManifold;
        worldManifold.Initialize(&manifold, xfA, brick.collision_shape.shapes[0].shape.m_radius, xfB, collision_shape.shapes[0].shape.m_radius);

        const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
        const glm::vec2 hitPos = { worldManifold.points[i].x, worldManifold.points[i].y };

        const float dy = collision_shape.shapes[0].shape.m_centroid.y - 0.5f * (hitPos.y - pos.y);
        const float d = -worldManifold.separations[i];

        if (normal.x != 0.f && dy <= minD)
          continue;

        pointIndex = i;
      }

      if (pointIndex >= 0)
      {
        b2WorldManifold worldManifold;
        worldManifold.Initialize(&manifold, xfA, collision_shape.shapes[0].shape.m_radius, xfB, brick.collision_shape.shapes[0].shape.m_radius);

        const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
        const glm::vec2 hitPos = { worldManifold.points[pointIndex].x, worldManifold.points[pointIndex].y };

        if (normal.y < 0.f)
          is_on_ground = true;

        const float d = -worldManifold.separations[pointIndex];
        const float proj = glm::dot(vel, normal);
        vel += glm::max(-proj, 0.f) * normal;
        pos += glm::max(d - minD, 0.f) * normal;
      }
    });
  }
};

struct render_debug_physics
{
  ECS_RUN(const RenderDebugStage &stage, const PhysicsWorld &phys_world)
  {
  #ifdef _DEBUG
    g_world->DrawDebugData();
  #endif
  }
};