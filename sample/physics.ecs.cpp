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
      assert(false);

    return true;
  }
}
DEF_COMP(PhysicsBody, phys_body);

DEF_SYS()
static __forceinline void init_physics_collision_handler(
  const EventOnEntityCreate &ev,
  const EntityId &eid,
  PhysicsBody &phys_body,
  CollisionShape &collision_shape)
{
  assert(phys_body.body != nullptr);

  for (auto s : collision_shape.shapes)
  {
    assert(s.shape.Validate());

    b2FixtureDef def;
    def.shape = &s.shape;
    def.density = s.density;
    def.friction = s.friction;
    def.isSensor = s.isSensor;
    def.userData = (void*)eid.handle;
    s.fixture = phys_body.body->CreateFixture(&def);
  }
}

DEF_SYS()
static __forceinline void init_physics_body_handler(
  const EventOnEntityCreate &ev,
  PhysicsBody &phys_body,
  const glm::vec2 &pos)
{
  assert(phys_body.body == nullptr);

  b2BodyDef def;
  def.type = phys_body.type;
  def.position.Set(pos.x, pos.y);

  phys_body.body = g_world->CreateBody(&def);
}

DEF_SYS()
static __forceinline void init_physics_world_handler(const EventOnEntityCreate &ev, PhysicsWorld &phys_world)
{
  assert(g_world == nullptr);

  g_world = new b2World({ 0.f, -9.81f });
  g_world->SetDebugDraw(&phys_world.debugDraw);
  phys_world.debugDraw.AppendFlags(b2Draw::e_shapeBit);
}

DEF_SYS()
static __forceinline void update_physics(const UpdateStage &stage, PhysicsWorld &phys_world)
{
  const float physDt = 1.f / 60.f;
  const int curTick = (int)ceil(stage.total / (double)physDt);

  if (curTick > phys_world.atTick)
  {
    g_world->Step(physDt, 3, 2);
    phys_world.atTick = curTick;
  }
}

DEF_SYS()
static __forceinline void update_kinematic_physics_body(
  const UpdateStage &stage,
  PhysicsBody &phys_body,
  const glm::vec2 &pos,
  const glm::vec2 &vel)
{
  assert(phys_body.type != b2_staticBody);

  phys_body.body->SetTransform({ pos.x, pos.y }, 0.f);
  phys_body.body->SetLinearVelocity({ vel.x, vel.y });
}

DEF_QUERY(BricksQuery, HAVE_COMP(wall));
DEF_QUERY(MovigBricksQuery, HAVE_COMP(wall) HAVE_COMP(auto_move));

DEF_SYS(HAVE_COMP(user_input))
static __forceinline void update_physics_collisions(
  const UpdateStage &stage,
  const PhysicsBody &phys_body,
  const CollisionShape &collision_shape,
  const Gravity &gravity,
  bool &is_on_ground,
  glm::vec2 &pos,
  glm::vec2 &vel)
{
  glm::vec2 myPos = pos;
  glm::vec2 myVel = vel;
  const CollisionShape &myCollisionShape = collision_shape;
  const PhysicsBody &myPhysBody = phys_body;

  MovigBricksQuery::exec([&](const CollisionShape &collision_shape, const glm::vec2 &pos, const glm::vec2 &vel)
  {
    b2Transform xfA = { { pos.x, pos.y }, b2Rot{0.f} };
    b2Transform xfB = { { myPos.x, myPos.y }, b2Rot{0.f} };

    b2Manifold manifold;
    b2CollidePolygons(&manifold, &collision_shape.shapes[1].shape, xfA, &myCollisionShape.shapes[0].shape, xfB);

    if (manifold.pointCount > 0 && glm::dot(myVel, vel) >= 0.f)
    {
      myVel += vel;
      myVel.y -= gravity.mass * 9.8f * stage.dt;
    }
  });

  BricksQuery::exec([&](const CollisionShape &collision_shape, const glm::vec2 &pos)
  {
    const float minD = myCollisionShape.shapes[0].shape.m_radius + collision_shape.shapes[0].shape.m_radius;

    b2Transform xfA = { { pos.x, pos.y }, b2Rot{0.f} };
    b2Transform xfB = { { myPos.x, myPos.y }, b2Rot{0.f} };

    b2Manifold manifold;
    b2CollidePolygons(&manifold, &collision_shape.shapes[0].shape, xfA, &myCollisionShape.shapes[0].shape, xfB);

    int pointIndex = -1;
    for (int i = 0; i < manifold.pointCount && pointIndex < 0; ++i)
    {
      b2WorldManifold worldManifold;
      worldManifold.Initialize(&manifold, xfA, collision_shape.shapes[0].shape.m_radius, xfB, myCollisionShape.shapes[0].shape.m_radius);

      const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
      const glm::vec2 hitPos = { worldManifold.points[i].x, worldManifold.points[i].y };

      const float dy = myCollisionShape.shapes[0].shape.m_centroid.y - 0.5f * (hitPos.y - myPos.y);
      const float d = -worldManifold.separations[i];

      if (normal.x != 0.f && dy <= minD)
        continue;

      pointIndex = i;
    }

    if (pointIndex >= 0)
    {
      b2WorldManifold worldManifold;
      worldManifold.Initialize(&manifold, xfA, myCollisionShape.shapes[0].shape.m_radius, xfB, collision_shape.shapes[0].shape.m_radius);

      const glm::vec2 normal = { worldManifold.normal.x, worldManifold.normal.y };
      const glm::vec2 hitPos = { worldManifold.points[pointIndex].x, worldManifold.points[pointIndex].y };

      if (normal.y < 0.f)
        is_on_ground = true;

      const float d = -worldManifold.separations[pointIndex];
      const float proj = glm::dot(myVel, normal);
      myVel += glm::max(-proj, 0.f) * normal;
      myPos += glm::max(d - minD, 0.f) * normal;
    }
  });

  pos = myPos;
  vel = myVel;
}

DEF_SYS(HAVE_COMP(user_input))
static __forceinline void physics_contact_handler(const EventOnPhysicsContact &ev, glm::vec2 &vel)
{
  // if (glm::dot(vel, ev.vel) >= 0.f)
  //   vel += ev.vel;
}

DEF_SYS()
static __forceinline void check_physics_contacts(const UpdateStage &stage, const PhysicsWorld &phys_world)
{
  for (b2Contact *c = g_world->GetContactList(); c; c = c->GetNext())
  {
    if (c->IsTouching())
    {
      if (c->GetFixtureA()->IsSensor())
      {
        // EntityId eidB = (uint32_t)c->GetFixtureB()->GetUserData();
        // const b2Vec2 &velA = c->GetFixtureA()->GetBody()->GetLinearVelocity();
        // g_mgr->sendEvent(eidB, EventOnPhysicsContact{ {velA.x, velA.y} });
      }
      else if (c->GetFixtureB()->IsSensor())
      {
        // EntityId eidA = (uint32_t)c->GetFixtureA()->GetUserData();
        // const b2Vec2 &velB = c->GetFixtureB()->GetBody()->GetLinearVelocity();
        // g_mgr->sendEvent(eidA, EventOnPhysicsContact{ {velB.x, velB.y} });
      }
    }
  }
}

DEF_SYS()
static __forceinline void render_debug_physics(const RenderDebugStage &stage, const PhysicsWorld &phys_world)
{
  g_world->DrawDebugData();
}