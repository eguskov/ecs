#include "boids.h"

#include <ecs/ecs.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <raylib.h>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <random>

#include "grid.h"
#include "update.h"

extern Camera2D camera;
extern int screen_width;
extern int screen_height;

static float COHESION = 3.f;
static float ALIGNMENT = 4.f;
static float WANDER = 0.5f;

static float RADIUS = 256.f;
static float SEPARATION_RADIUS = 32.f;

struct Boid
{
  ECS_QUERY;

  QL_HAVE(boid);

  QL_INDEX(grid_cell);

  const EntityId &eid;
  const glm::vec2 &pos;
  const glm::vec2 &vel;
  float mass;
};

struct on_mouse_click_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnClickMouseLeftButton &ev)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    JFrameAllocator alloc;
    JFrameValue comps(rapidjson::kObjectType);
    {
      JFrameValue arr(rapidjson::kArrayType);
      arr.PushBack(( ev.pos.x - hw) * (1.f / camera.zoom), alloc);
      arr.PushBack((-ev.pos.y + hh) * (1.f / camera.zoom), alloc);
      comps.AddMember("pos", eastl::move(arr), alloc);
    }
    {
      // float mass = 1.f + (3.f - (1.f)) * float(::rand())/float(RAND_MAX);
      // comps.AddMember("mass", mass, alloc);
    }
    g_mgr->createEntity("boid", comps);
  }
};

struct on_click_space_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnClickSpace &ev)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    JFrameAllocator alloc;
    JFrameValue comps(rapidjson::kObjectType);
    {
      JFrameValue arr(rapidjson::kArrayType);
      arr.PushBack(( ev.pos.x - hw) * (1.f / camera.zoom), alloc);
      arr.PushBack((-ev.pos.y + hh) * (1.f / camera.zoom), alloc);
      comps.AddMember("pos", eastl::move(arr), alloc);
    }
    {
      // float mass = 1.f + (3.f - (1.f)) * float(::rand())/float(RAND_MAX);
      comps.AddMember("mass", 3.f, alloc);
    }
    g_mgr->createEntity("boid", comps);
  }
};

struct on_change_cohesion_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeCohesion &ev)
  {
    COHESION = glm::max(COHESION + ev.delta, 0.25f);
  }
};

struct on_change_alignment_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeAlignment &ev)
  {
    ALIGNMENT = glm::max(ALIGNMENT + ev.delta, 0.25f);
  }
};

struct on_change_wander_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeWander &ev)
  {
    WANDER = glm::max(WANDER + ev.delta, 0.25f);
  }
};

struct render_hud_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const RenderHUDStage &stage)
  {
    DrawText(FormatText("Cohesion: %2.2f", COHESION), 10, 90, 20, LIME);
    DrawText(FormatText("Alignment: %2.2f", ALIGNMENT), 10, 110, 20, LIME);
    DrawText(FormatText("Wander: %2.2f", WANDER), 10, 130, 20, LIME);
  }
};

struct render_boid
{
  QL_HAVE(boid);

  ECS_RUN(const RenderStage &stage, const TextureAtlas &texture, const glm::vec4 &frame, const glm::vec2 &pos, const glm::vec2 &vel, float mass, float rotation)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    Rectangle rect = {frame.x, frame.y, frame.z, frame.w};
    Rectangle destRec = {hw + pos.x, hh - pos.y, fabsf(frame.z) * mass, fabsf(frame.w) * mass};

    DrawTexturePro(texture.id, rect, destRec, Vector2{0.5f * frame.z, 0.5f * frame.w}, -rotation, WHITE);
  }
};

struct render_boid_debug
{
  QL_HAVE(boid);

  QL_WHERE(grid_cell != -1);

  ECS_RUN(const RenderDebugStage &stage, const EntityId &eid, const glm::vec2 &pos, const glm::vec2 &flock_center, int grid_cell)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    DrawCircleLines(int(hw + pos.x), int(hh - pos.y), RADIUS, CLITERAL{ 255, 0, 0, 150 });
    DrawCircleLines(int(hw + pos.x), int(hh - pos.y), SEPARATION_RADIUS, CLITERAL{ 0, 255, 0, 150 });
    DrawCircleV(Vector2{hw + flock_center.x, hh - flock_center.y}, 10.f, CLITERAL{ 0, 0, 255, 150 });

    glm::vec2 box(128.f, 128.f);
    const int boxCellLeft = MAKE_GRID_INDEX(pos.x - box.x);
    const int boxCellTop = MAKE_GRID_INDEX(pos.y + box.y);
    const int boxCellRight = MAKE_GRID_INDEX(pos.x + box.x);
    const int boxCellBottom = MAKE_GRID_INDEX(pos.y - box.y);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellBottom; y <= boxCellTop; ++y)
      {
        DrawRectangleV(Vector2{hw + float(x) * float(GRID_CELL_SIZE), hh - float(y) * float(GRID_CELL_SIZE)}, Vector2{float(GRID_CELL_SIZE), float(GRID_CELL_SIZE)}, CLITERAL{ 0, 117, 44, 28 });

        if (Query *cell = Boid::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
          {
            Boid boid = Boid::get(q);
            if (boid.eid != eid)
              DrawCircleV(Vector2{hw + boid.pos.x, hh - boid.pos.y}, 5.f, CLITERAL{ 0, 0, 255, 150 });
          }
      }
  }
};

struct update_boid_position
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * stage.dt;
  }
};

struct update_boid_rotation
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, float &rotation)
  {
    const glm::vec2 dir = glm::normalize(vel);
    rotation = glm::degrees(glm::acos(dir.x)) * glm::sign(vel.y);
  }
};

struct update_boid
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &pos, const glm::vec2 &vel, float mass, glm::vec2 &force)
  {
    const float hw = 0.5f * screen_width * (1.f / camera.zoom);
    const float hh = 0.5f * screen_height * (1.f / camera.zoom);

    glm::vec2 targetVel = vel;

    glm::vec2 futurePos = pos + vel * 1.5f * mass;
    if (futurePos.x >= hw || futurePos.x <= -hw || futurePos.y >= hh || futurePos.y <= -hh)
      targetVel = glm::vec2(-vel.y, vel.x);

    glm::vec2 steer = targetVel - vel;
    force += steer * 5.0f;
  }
};

struct update_boid_wander
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &force, glm::vec2 &wander_vel, float &wander_timer)
  {
    wander_timer -= stage.dt;
    if (wander_timer <= 0.f)
    {
      wander_timer = float(::rand())/float(RAND_MAX) + 1.f;
      float rndX = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      float rndY = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      wander_vel = 150.f * glm::normalize(glm::vec2(rndX, rndY));
    }

    glm::vec2 steer = wander_vel - vel;
    force += steer * WANDER;
  }
};

struct update_boid_separation
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const EntityId &eid, const glm::vec2 &pos, const glm::vec2 &vel, float mass, glm::vec2 &force)
  {
    glm::vec2 box(128.f, 128.f);
    const int boxCellLeft = MAKE_GRID_INDEX(pos.x - box.x);
    const int boxCellTop = MAKE_GRID_INDEX(pos.y + box.y);
    const int boxCellRight = MAKE_GRID_INDEX(pos.x + box.x);
    const int boxCellBottom = MAKE_GRID_INDEX(pos.y - box.y);

    float flockmatesWeight = 0.f;
    glm::vec2 separationDir(0.f, 0.f);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellBottom; y <= boxCellTop; ++y)
        if (Query *cell = Boid::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
          {
            Boid boid = Boid::get(q);
            if (boid.eid != eid)
            {
              const float dist = glm::length(pos - boid.pos);
              if (dist < boid.mass * SEPARATION_RADIUS && boid.mass >= mass)
              {
                flockmatesWeight += boid.mass;
                separationDir += boid.mass * (pos - boid.pos);
              }
            }
          }

    if (flockmatesWeight > 0.f && glm::length(separationDir) > 0.f)
    {
      separationDir = 150.f * glm::normalize(separationDir / flockmatesWeight);
      glm::vec2 steer = separationDir - vel;
      force += steer * 50.0f;
    }
  }
};

struct update_boid_alignment
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const EntityId &eid, const glm::vec2 &pos, const glm::vec2 &vel, float mass, glm::vec2 &force)
  {
    glm::vec2 box(128.f, 128.f);
    const int boxCellLeft = MAKE_GRID_INDEX(pos.x - box.x);
    const int boxCellTop = MAKE_GRID_INDEX(pos.y + box.y);
    const int boxCellRight = MAKE_GRID_INDEX(pos.x + box.x);
    const int boxCellBottom = MAKE_GRID_INDEX(pos.y - box.y);

    float flockmatesWeight = 0.f;
    glm::vec2 flockDir(0.f, 0.f);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellBottom; y <= boxCellTop; ++y)
        if (Query *cell = Boid::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
          {
            Boid boid = Boid::get(q);
            const float dist = glm::length(pos - boid.pos);
            if (boid.eid != eid && dist < RADIUS && boid.mass >= mass)
            {
              flockmatesWeight += boid.mass;
              flockDir += boid.mass * boid.vel;
            }
          }

    if (flockmatesWeight > 0.f && glm::length(flockDir) > 0.f)
    {
      flockDir /= flockmatesWeight;
      glm::vec2 steer = 150.f * glm::normalize(flockDir - vel);
      force += steer * ALIGNMENT;
    }
  }
};

struct update_boid_cohesion
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, const EntityId &eid, const glm::vec2 &pos, float mass, glm::vec2 &flock_center, glm::vec2 &force)
  {
    glm::vec2 box(128.f, 128.f);
    const int boxCellLeft = MAKE_GRID_INDEX(pos.x - box.x);
    const int boxCellTop = MAKE_GRID_INDEX(pos.y + box.y);
    const int boxCellRight = MAKE_GRID_INDEX(pos.x + box.x);
    const int boxCellBottom = MAKE_GRID_INDEX(pos.y - box.y);

    float flockmatesWeight = 0.f;
    glm::vec2 center(0.f, 0.f);

    for (int x = boxCellLeft; x <= boxCellRight; ++x)
      for (int y = boxCellBottom; y <= boxCellTop; ++y)
        if (Query *cell = Boid::index()->find(MAKE_GRID_CELL(x, y)))
          for (auto q = cell->begin(), e = cell->end(); q != e; ++q)
          {
            Boid boid = Boid::get(q);
            const float dist = glm::length(pos - boid.pos);
            if (boid.eid != eid && dist < RADIUS && boid.mass >= mass)
            {
              flockmatesWeight += boid.mass;
              center += boid.mass * boid.pos;
            }
          }

    if (flockmatesWeight > 0.f)
    {
      center /= flockmatesWeight;
      flock_center = center;
      glm::vec2 steer = 150.f * glm::normalize(center - pos);
      force += steer * COHESION;
    }
  }
};

struct control_boid_velocity
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, glm::vec2 &vel)
  {
    if (glm::length(vel) == 0.f)
      vel.x = 150.f;
    vel = 150.f * glm::normalize(vel);
  }
};

struct apply_boid_force
{
  QL_HAVE(boid);

  ECS_RUN(const UpdateStage &stage, float mass, glm::vec2 &force, glm::vec2 &vel)
  {
    glm::vec2 dv = force * (1.f / mass) * stage.dt;
    if (glm::length(dv) > 20.f)
      dv = 20.f * glm::normalize(dv);
    vel += dv;
    force = glm::vec2(0.f, 0.f);
  }
};