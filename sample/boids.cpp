#include "boids.h"

#include <ecs/ecs.h>
#include <ecs/jobmanager.h>

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

constexpr int GRID_SIZE = 256;

static float COHESION = 100.f;
static float ALIGNMENT = 200.f;
static float WANDER = 100.0f;
static float SEPARATION = 300.0f;
static float AVOID_WALLS = 500.0f;
static float AVOID_OBSTACLE = 1000.0f;

static float COHESION_RADIUS = 128.f;
static float ALIGNMENT_RADIUS = 128.f;
static float SEPARATION_RADIUS = 64.f;
static float OBSTACLE_RADIUS = 200.f;

struct Boid
{
  ECS_QUERY;

  QL_HAVE(boid);

  const glm::vec2 &pos;
  const glm::vec2 &vel;
  float mass;
};

struct BoidObstacle
{
  ECS_QUERY;

  QL_HAVE(boid_obstacle);

  const glm::vec2 &pos;
};

struct on_mouse_click_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnClickMouseLeftButton &ev)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    const float invZoom = (1.f / camera.zoom);
    for (int i = 0; i < 1000; ++i)
    {
      ComponentsMap cmap;
      cmap.add(HASH("pos"), glm::vec2((ev.pos.x - hw) * invZoom, (-ev.pos.y + hh) * invZoom));
      ecs::create_entity("boid", eastl::move(cmap));
    }
  }
};

struct on_click_space_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnClickSpace &ev)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    const float invZoom = (1.f / camera.zoom);
    ComponentsMap cmap;
    cmap.add(HASH("pos"), glm::vec2((ev.pos.x - hw) * invZoom, (-ev.pos.y + hh) * invZoom));
    cmap.add(HASH("mass"), 3.f);
    ecs::create_entity("boid", eastl::move(cmap));
  }
};

struct on_click_left_control_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnClickLeftControl &ev)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;

    const float invZoom = (1.f / camera.zoom);
    ComponentsMap cmap;
    cmap.add(HASH("pos"), glm::vec2((ev.pos.x - hw) * invZoom, (-ev.pos.y + hh) * invZoom));
    ecs::create_entity("boid_obstacle", eastl::move(cmap));
  }
};

struct on_change_cohesion_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeCohesion &ev)
  {
    COHESION = glm::max(COHESION + ev.delta, 0.0f);
  }
};

struct on_change_alignment_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeAlignment &ev)
  {
    ALIGNMENT = glm::max(ALIGNMENT + ev.delta, 0.0f);
  }
};

struct on_change_separation_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeSeparation &ev)
  {
    SEPARATION = glm::max(SEPARATION + ev.delta, 0.0f);
  }
};

struct on_change_wander_handler_boid
{
  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventOnChangeWander &ev)
  {
    WANDER = glm::max(WANDER + ev.delta, 0.0f);
  }
};

struct render_hud_boid
{
  ECS_AFTER(after_render);

  QL_HAVE(click_handler_boid);

  ECS_RUN(const EventRenderHUD &evt)
  {
    DrawText(FormatText("Cohesion: %2.2f", COHESION), 10, 90, 20, LIME);
    DrawText(FormatText("Alignment: %2.2f", ALIGNMENT), 10, 110, 20, LIME);
    DrawText(FormatText("Separaion: %2.2f", SEPARATION), 10, 130, 20, LIME);
    DrawText(FormatText("Wander: %2.2f", WANDER), 10, 150, 20, LIME);
  }
};

struct render_boid_obstacle
{
  ECS_AFTER(before_render);
  ECS_BEFORE(after_render);

  QL_HAVE(boid_obstacle);

  ECS_RUN(const EventRender &evt, const TextureAtlas &texture, const glm::vec4 &frame, const glm::vec2 &pos)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    Rectangle rect = {frame.x, frame.y, frame.z, frame.w};
    Rectangle destRec = {hw + pos.x, hh - pos.y, fabsf(frame.z) * 5.f, fabsf(frame.w) * 5.f};

    DrawTexturePro(texture.id, rect, destRec, Vector2{0.5f * frame.z * 5.f, 0.5f * frame.w * 5.f}, 0.f, WHITE);
  }
};

struct render_boid
{
  ECS_AFTER(before_render);
  ECS_BEFORE(after_render);

  QL_HAVE(boid);

  ECS_RUN(const EventRender &evt, const TextureAtlas &texture, const glm::vec4 &frame, const glm::vec2 &cur_pos, const glm::vec2 &cur_separation_center, const glm::vec2 &cur_cohesion_center, float mass, float cur_rotation)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    Rectangle rect = {frame.x, frame.y, frame.z, frame.w};
    Rectangle destRec = {hw + cur_pos.x, hh - cur_pos.y, fabsf(frame.z) * mass, fabsf(frame.w) * mass};

    DrawTexturePro(texture.id, rect, destRec, Vector2{0.5f * frame.z * mass, 0.5f * frame.w * mass}, -cur_rotation, WHITE);

    // Draw separation
    // DrawCircleV(Vector2{hw + cur_separation_center.x, hh - cur_separation_center.y}, 10.f, CLITERAL{ 255, 0, 255, 255 });
    // DrawLineV(Vector2{destRec.x, destRec.y}, Vector2{hw + cur_separation_center.x, hh - cur_separation_center.y}, CLITERAL{ 255, 0, 255, 255 });
    // DrawCircleLines(int(destRec.x), int(destRec.y), SEPARATION_RADIUS, CLITERAL{ 0, 255, 0, 150 });

    // Draw cohesion
    // DrawCircleV(Vector2{hw + cur_cohesion_center.x, hh - cur_cohesion_center.y}, 10.f, CLITERAL{ 255, 0, 255, 255 });
    // DrawLineV(Vector2{destRec.x, destRec.y}, Vector2{hw + cur_cohesion_center.x, hh - cur_cohesion_center.y}, CLITERAL{ 255, 0, 255, 255 });
    // DrawCircleLines(int(destRec.x), int(destRec.y), COHESION_RADIUS, CLITERAL{ 0, 255, 0, 150 });

    // glm::vec2 cellPos(MAKE_GRID_INDEX(cur_pos.x, GRID_SIZE) * float(GRID_SIZE), (MAKE_GRID_INDEX(cur_pos.y + float(GRID_SIZE), GRID_SIZE)) * float(GRID_SIZE));
    // DrawRectangleV(Vector2{hw + cellPos.x, hh - cellPos.y}, Vector2{float(GRID_SIZE), float(GRID_SIZE)}, CLITERAL{ 0, 0, 255, 50 });

    // glm::vec2 box(SEPARATION_RADIUS, SEPARATION_RADIUS);
    // for (int x = MAKE_GRID_INDEX(cur_pos.x - box.x, GRID_SIZE); x <= MAKE_GRID_INDEX(cur_pos.x + box.x, GRID_SIZE); ++x)
    //   for (int y = MAKE_GRID_INDEX(cur_pos.y + float(GRID_SIZE) - box.y, GRID_SIZE); y <= MAKE_GRID_INDEX(cur_pos.y + float(GRID_SIZE) + box.y, GRID_SIZE); ++y)
    //   {
    //     glm::vec2 cellPos(x * float(GRID_SIZE), y * float(GRID_SIZE));
    //     DrawRectangleV(Vector2{hw + cellPos.x, hh - cellPos.y}, Vector2{float(GRID_SIZE), float(GRID_SIZE)}, CLITERAL{ 0, 0, 255, 50 });
    //   }
  }
};

struct copy_boid_state
{
  QL_HAVE(boid);

  ECS_RUN(
    const EventUpdate &stage,
    const glm::vec2 &pos,
    const glm::vec2 &separation_center,
    const glm::vec2 &cohesion_center,
    const glm::vec2 &alignment_dir,
    float rotation,
    glm::vec2 &cur_pos,
    float &cur_rotation,
    glm::vec2 &cur_separation_center,
    glm::vec2 &cur_cohesion_center,
    glm::vec2 &cur_alignment_dir)
  {
    cur_pos = pos;
    cur_rotation = rotation;
    cur_separation_center = separation_center;
    cur_cohesion_center = cohesion_center;
    cur_alignment_dir = alignment_dir;
  }
};

struct update_boid_position
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * evt.dt;
  }
};

struct update_boid_rotation
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, float &rotation)
  {
    const glm::vec2 dir = glm::normalize(vel);
    rotation = glm::degrees(glm::acos(dir.x)) * glm::sign(vel.y);
  }
};

struct update_boid_avoid_walls
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &pos, const glm::vec2 &vel, float mass, float &move_to_center_timer, glm::vec2 &force)
  {
    const float hw = 0.5f * screen_width * (1.f / camera.zoom);
    const float hh = 0.5f * screen_height * (1.f / camera.zoom);

    glm::vec2 targetVel = vel;

    glm::vec2 futurePos = pos + vel * 1.5f * mass;
    if (futurePos.x >= hw || futurePos.x <= -hw || futurePos.y >= hh || futurePos.y <= -hh)
    {
      move_to_center_timer = 1.f;
      targetVel = glm::vec2(-vel.y, vel.x);
    }

    glm::vec2 steer = targetVel - vel;
    if (glm::length(steer) > 0.f)
      force += glm::normalize(steer) * AVOID_WALLS;
  }
};

struct update_boid_avoid_obstacle
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &pos, glm::vec2 &force)
  {
    BoidObstacle::foreach([&](BoidObstacle &&obstacle)
    {
      const float dist = glm::length(obstacle.pos - pos);
      if (dist <= OBSTACLE_RADIUS)
        force += glm::normalize(pos - obstacle.pos) * AVOID_OBSTACLE;
    });
  }
};

struct update_boid_move_to_center
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &pos, float &move_to_center_timer, glm::vec2 &force)
  {
    if (move_to_center_timer > 0.f)
    {
      move_to_center_timer -= evt.dt;

      glm::vec2 center(0.f, 0.f);
      glm::vec2 steer = center - pos;
      if (glm::length(steer) > 0.f)
        force += glm::normalize(steer) * AVOID_WALLS;
    }
  }
};

struct update_boid_wander
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, glm::vec2 &force, glm::vec2 &wander_vel, float &wander_timer)
  {
    wander_timer -= evt.dt;
    if (wander_timer <= 0.f)
    {
      wander_timer = float(::rand())/float(RAND_MAX) + 1.f;
      float rndX = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      float rndY = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      wander_vel = 150.f * glm::normalize(glm::vec2(rndX, rndY));
    }

    glm::vec2 steer = wander_vel - vel;
    if (glm::length(steer) > 0.f)
      force += glm::normalize(steer) * WANDER;
  }
};

struct control_boid_velocity
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, float max_vel, glm::vec2 &vel)
  {
    if (glm::length(vel) == 0.f)
    {
      float rndX = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      float rndY = -1.f + (1.f - (-1.f)) * float(::rand())/float(RAND_MAX);
      vel = glm::vec2(rndX, rndY);
    }
    vel = max_vel * glm::normalize(vel);
  }
};

struct apply_boid_force
{
  QL_HAVE(boid);

  ECS_ADD_JOBS(jobmanager::DependencyList &&deps, jobmanager::callback_t &&task, int count)
  {
    return jobmanager::add_job(eastl::move(deps), count, 256, eastl::move(task));
  }

  ECS_RUN(const EventUpdate &evt, float mass, glm::vec2 &force, glm::vec2 &vel)
  {
    vel += force * (1.f / mass) * evt.dt;
    force = glm::vec2(0.f, 0.f);
  }
};

struct BoidSeparation
{
  ECS_QUERY;

  QL_HAVE(boid);

  const EntityId &eid;
  const glm::vec2 &pos;
  const glm::vec2 &vel;
  glm::vec2 &force;
  glm::vec2 &separation_center;
  glm::vec2 &cohesion_center;
  glm::vec2 &alignment_dir;
};

#define GRID_BOIDS 1

#if GRID_BOIDS

struct update_boid_rules
{
  struct StaticData
  {
    std::mutex boidsMapMutex;
    eastl::hash_multimap<uint32_t, int> boidsMap;
  };

  static StaticData sd;

  static void addBoidToMap(uint32_t grid_cell, int index)
  {
    std::lock_guard<std::mutex> lock(sd.boidsMapMutex);
    auto res = sd.boidsMap.insert(grid_cell);
    res->second = index;
  }

  ECS_RUN_T(const EventUpdate &evt, QueryIterable<BoidSeparation, _> &&boids)
  {
    using Iter = QueryIterable<BoidSeparation, _>;

    const SystemId sid = ecs::get_system_id(HASH("update_boid_rules"));

    jobmanager::DependencyList deps = ecs::get_system_dependency_list(sid);

    using Vec2List = eastl::vector<glm::vec2, FrameMemAllocator>;
    using IntList = eastl::vector<int, FrameMemAllocator>;

    Vec2List *cellSeparation = new (alloc_frame_mem(sizeof(Vec2List))) Vec2List();
    cellSeparation->resize(boids.count());

    Vec2List *cellAlignment = new (alloc_frame_mem(sizeof(Vec2List))) Vec2List();
    cellAlignment->resize(boids.count());

    IntList *cellIndices = new (alloc_frame_mem(sizeof(IntList))) IntList();
    cellIndices->resize(boids.count(), -1);

    IntList *cellCount = new (alloc_frame_mem(sizeof(IntList))) IntList();
    cellCount->resize(boids.count(), 1);

    sd.boidsMap.clear();
    sd.boidsMap.reserve(boids.count());

    auto boidsBegin = boids.first;
    auto addBoidsToMapJob = jobmanager::add_job(deps, boids.count(), 256, [boidsBegin](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
      {
        BoidSeparation boid(Iter::get(q));
        const uint32_t gridCell = MAKE_GRID_CELL_FROM_POS(boid.pos + glm::vec2(0.f, float(GRID_SIZE)), GRID_SIZE);
        addBoidToMap(gridCell, i);
      }
    });

    auto initCellSeparationJob = jobmanager::add_job(deps, boids.count(), 256, [boidsBegin, cellSeparation](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
        (*cellSeparation)[i] = Iter::get(q).pos;
    });

    auto initCellAlignmentJob = jobmanager::add_job(deps, boids.count(), 256, [boidsBegin, cellAlignment](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
        (*cellAlignment)[i] = Iter::get(q).vel;
    });

    auto initBarrier = jobmanager::add_job({ addBoidsToMapJob, initCellSeparationJob, initCellAlignmentJob });

    auto mergeCellsJob = jobmanager::add_job({ initBarrier }, sd.boidsMap.bucket_count(), 1, [cellIndices, cellCount, cellSeparation, cellAlignment](int bucket, int)
    {
      eastl::hash_map<uint32_t, int> firstIndexMap;
      firstIndexMap.reserve(sd.boidsMap.bucket_size(bucket));
      for (auto i = sd.boidsMap.begin(bucket), e = sd.boidsMap.end(bucket); i != e; ++i)
      {
        const uint32_t gridCell = i->first;
        const int index = i->second;

        auto firstIndex = firstIndexMap.find(gridCell);
        if (firstIndex == firstIndexMap.end())
        {
          firstIndexMap.insert(eastl::pair<uint32_t, int>(gridCell, index));

          (*cellIndices)[index] = index;
        }
        else
        {
          const int cellIndex = firstIndex->second;

          (*cellCount)[cellIndex] += 1;
          (*cellAlignment)[cellIndex] = (*cellAlignment)[cellIndex] + (*cellAlignment)[index];
          (*cellSeparation)[cellIndex] = (*cellSeparation)[cellIndex] + (*cellSeparation)[index];

          (*cellIndices)[index] = cellIndex;
        }
      }
    });

    auto steerJob = jobmanager::add_job({ mergeCellsJob }, boids.count(), 256, [boidsBegin, cellIndices, cellCount, cellAlignment, cellSeparation](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
      {
        BoidSeparation boid(Iter::get(q));

        glm::vec2 box(SEPARATION_RADIUS, SEPARATION_RADIUS);

        glm::vec2 separationCenter(0.f, 0.f);
        int separationNeighborCount = 0;

        for (int x = MAKE_GRID_INDEX(boid.pos.x - box.x, GRID_SIZE); x <= MAKE_GRID_INDEX(boid.pos.x + box.x, GRID_SIZE); ++x)
          for (int y = MAKE_GRID_INDEX(boid.pos.y + float(GRID_SIZE) - box.y, GRID_SIZE); y <= MAKE_GRID_INDEX(boid.pos.y + float(GRID_SIZE) + box.y, GRID_SIZE); ++y)
          {
            auto res = sd.boidsMap.find(MAKE_GRID_CELL(x, y));
            if (res != sd.boidsMap.end())
            {
              const int cellIndex = (*cellIndices)[res->second];
              const int neighborCount = (*cellCount)[cellIndex];

              separationCenter += (*cellSeparation)[cellIndex];
              separationNeighborCount += (*cellCount)[cellIndex];
            }
          }

        separationCenter /= float(separationNeighborCount);

        const int cellIndex = (*cellIndices)[i];
        const int neighborCount = (*cellCount)[cellIndex];

        glm::vec2 cohesionCenter = (*cellSeparation)[cellIndex] / float(neighborCount);

        glm::vec2 separation = boid.pos - separationCenter;
        const float separationLen = glm::length(separation);
        if (separationLen > 0.f && separationLen <= SEPARATION_RADIUS)
          boid.force += (separation / separationLen) * SEPARATION;

        glm::vec2 cohesion = cohesionCenter - boid.pos;
        const float cohesionLen = glm::length(cohesion);
        if (cohesionLen > 0.f)
          boid.force += (cohesion / cohesionLen) * COHESION;

        glm::vec2 alignment = ((*cellAlignment)[cellIndex] / float(neighborCount)) - boid.vel;
        const float alignmentLen = glm::length(alignment);
        if (alignmentLen > 0.f)
          boid.force += (alignment / alignmentLen) * ALIGNMENT;

        boid.separation_center = separationCenter;
        boid.cohesion_center = cohesionCenter;
        boid.alignment_dir = (*cellAlignment)[i] / float(neighborCount);
      }
    });

    ecs::set_system_job(sid, steerJob);

    jobmanager::start_jobs();
  }
};

update_boid_rules::StaticData update_boid_rules::sd;

#else

struct update_boid_rules
{
  struct Data
  {
    glm::vec2 pos;
    glm::vec2 vel;
    int32_t index;

    inline bool operator<(const Data &rhs) const
    {
      return glm::length(pos) < glm::length(rhs.pos);
    }
  };

  ECS_RUN_T(const EventUpdate &evt, QueryIterable<BoidSeparation, _> &&boids)
  {
    using Iter = QueryIterable<BoidSeparation, _>;

    const SystemId sid = ecs::get_system_id(HASH("update_boid_rules"));

    jobmanager::DependencyList deps = ecs::get_system_dependency_list(sid);

    using BoidsDataVector = eastl::vector<Data, FrameMemAllocator>;
    BoidsDataVector *boidsData = new (alloc_frame_mem(sizeof(BoidsDataVector))) BoidsDataVector();
    boidsData->resize(boids.count());

    using Vec2List = eastl::vector<glm::vec2, FrameMemAllocator>;
    Vec2List *boidsSeparation = new (alloc_frame_mem(sizeof(Vec2List))) Vec2List();
    boidsSeparation->resize(boids.count(), glm::vec2(0.f, 0.f));

    Vec2List *boidsAlignment = new (alloc_frame_mem(sizeof(Vec2List))) Vec2List();
    boidsAlignment->resize(boids.count(), glm::vec2(0.f, 0.f));

    Vec2List *boidsCohesion = new (alloc_frame_mem(sizeof(Vec2List))) Vec2List();
    boidsCohesion->resize(boids.count(), glm::vec2(0.f, 0.f));

    using IntList = eastl::vector<int, FrameMemAllocator>;
    IntList *boidsSeparationCount = new (alloc_frame_mem(sizeof(IntList))) IntList();
    boidsSeparationCount->resize(boids.count(), 1);

    IntList *boidsAlignmentCount = new (alloc_frame_mem(sizeof(IntList))) IntList();
    boidsAlignmentCount->resize(boids.count(), 1);

    IntList *boidsCohesionCount = new (alloc_frame_mem(sizeof(IntList))) IntList();
    boidsCohesionCount->resize(boids.count(), 1);

    auto boidsBegin = boids.first;
    auto copyDataJob = jobmanager::add_job(deps, boids.count(), 256, [boidsBegin, boidsData](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
      {
        BoidSeparation boid(Iter::get(q));
        (*boidsData)[i].pos = boid.pos;
        (*boidsData)[i].vel = boid.vel;
        (*boidsData)[i].index = i;
      }
    });

    auto sortingJob = jobmanager::add_job({ copyDataJob }, boids.count(), boids.count(), [boidsData](int from, int count)
    {
      eastl::quick_sort(boidsData->begin(), boidsData->end());
    });

    auto initSeparationJob = jobmanager::add_job({ sortingJob }, boids.count(), 256, [boidsData, boidsSeparation](int from, int count)
    {
      int i = from;
      for (int i = from; i < from + count; ++i)
        (*boidsSeparation)[i] = (*boidsData)[i].pos;
    });

    auto initCohesionJob = jobmanager::add_job({ sortingJob }, boids.count(), 256, [boidsData, boidsCohesion](int from, int count)
    {
      int i = from;
      for (int i = from; i < from + count; ++i)
        (*boidsCohesion)[i] = (*boidsData)[i].pos;
    });

    auto initAlignmentJob = jobmanager::add_job({ sortingJob }, boids.count(), 256, [boidsData, boidsAlignment](int from, int count)
    {
      for (int i = from; i < from + count; ++i)
        (*boidsAlignment)[i] = (*boidsData)[i].vel;
    });

    auto initBarrier = jobmanager::add_job({ initAlignmentJob, initSeparationJob, initCohesionJob });

    auto separationJob = jobmanager::add_job({ initBarrier }, boids.count(), 256, [boidsData, boidsSeparation, boidsSeparationCount](int from, int count)
    {
      for (int i = from; i < from + count; ++i)
      {
        const Data &cur = (*boidsData)[i];
        const float curLen = glm::length(cur.pos);

        for (int n = i + 1; n < from + count; ++n)
        {
          const Data &next = (*boidsData)[n];
          const float nextLen = glm::length(next.pos);
          if (nextLen > curLen + SEPARATION_RADIUS)
            break;
          if (glm::distance(cur.pos, next.pos) <= SEPARATION_RADIUS)
          {
            ++(*boidsSeparationCount)[i];
            (*boidsSeparation)[i] += next.pos;
          }
        }
        for (int p = i - 1; p >= from; --p)
        {
          const Data &prev = (*boidsData)[p];
          const float prevLen = glm::length(prev.pos);
          if (prevLen < curLen - SEPARATION_RADIUS)
            break;
          if (glm::distance(cur.pos, prev.pos) <= SEPARATION_RADIUS)
          {
            ++(*boidsSeparationCount)[i];
            (*boidsSeparation)[i] += prev.pos;
          }
        }
        if ((*boidsSeparationCount)[i] >= 32)
          break;
      }
      for (int i = from; i < from + count; ++i)
        (*boidsSeparation)[i] /= float((*boidsSeparationCount)[i]);
    });

    auto cohesionJob = jobmanager::add_job({ initBarrier }, boids.count(), 256, [boidsData, boidsCohesion, boidsCohesionCount](int from, int count)
    {
      for (int i = from; i < from + count; ++i)
      {
        const Data &cur = (*boidsData)[i];
        const float curLen = glm::length(cur.pos);
        for (int n = i + 1; n < from + count; ++n)
        {
          const Data &next = (*boidsData)[n];
          const float nextLen = glm::length(next.pos);
          if (nextLen > curLen + COHESION_RADIUS)
            break;
          if (glm::distance(cur.pos, next.pos) <= COHESION_RADIUS)
          {
            ++(*boidsCohesionCount)[i];
            (*boidsCohesion)[i] += next.pos;
          }
        }
        for (int p = i - 1; p >= from; --p)
        {
          const Data &prev = (*boidsData)[p];
          const float prevLen = glm::length(prev.pos);
          if (prevLen < curLen - COHESION_RADIUS)
            break;
          if (glm::distance(cur.pos, prev.pos) <= COHESION_RADIUS)
          {
            ++(*boidsCohesionCount)[i];
            (*boidsCohesion)[i] += prev.pos;
          }
        }
        if ((*boidsCohesionCount)[i] >= 32)
          break;
      }
      for (int i = from; i < from + count; ++i)
        (*boidsCohesion)[i] /= float((*boidsCohesionCount)[i]);
    });

    auto alignmentJob = jobmanager::add_job({ initBarrier }, boids.count(), 256, [boidsData, boidsAlignment, boidsAlignmentCount](int from, int count)
    {
      for (int i = from; i < from + count; ++i)
      {
        const Data &cur = (*boidsData)[i];
        const float curLen = glm::length(cur.pos);
        for (int n = i + 1; n < from + count; ++n)
        {
          const Data &next = (*boidsData)[n];
          const float nextLen = glm::length(next.pos);
          if (nextLen > curLen + ALIGNMENT_RADIUS)
            break;
          if (glm::distance(cur.pos, next.pos) <= ALIGNMENT_RADIUS)
          {
            ++(*boidsAlignmentCount)[i];
            (*boidsAlignment)[i] += next.vel;
          }
        }
        for (int p = i - 1; p >= from; --p)
        {
          const Data &prev = (*boidsData)[p];
          const float prevLen = glm::length(prev.pos);
          if (prevLen < curLen - ALIGNMENT_RADIUS)
            break;
          if (glm::distance(cur.pos, prev.pos) <= ALIGNMENT_RADIUS)
          {
            ++(*boidsAlignmentCount)[i];
            (*boidsAlignment)[i] += prev.vel;
          }
        }
        if ((*boidsAlignmentCount)[i] >= 32)
          break;
      }
      for (int i = from; i < from + count; ++i)
        (*boidsAlignment)[i] /= float((*boidsAlignmentCount)[i]);
    });

    auto rulesBarrier = jobmanager::add_job({ alignmentJob, separationJob, cohesionJob });

    auto steerJob = jobmanager::add_job({ rulesBarrier }, boids.count(), 256, [boidsBegin, boidsData, boidsSeparation, boidsAlignment, boidsCohesion](int from, int count)
    {
      int i = from;
      for (auto q = boidsBegin + from, e = q + count; q != e; ++q, ++i)
      {
        const int index = (*boidsData)[i].index;

        BoidSeparation boid(Iter::get(boidsBegin + index));

        boid.separation_center = (*boidsSeparation)[i];
        boid.cohesion_center = (*boidsCohesion)[i];
        boid.alignment_dir = (*boidsAlignment)[i];

        glm::vec2 separation = boid.pos - (*boidsSeparation)[i];
        const float separationLen = glm::length(separation);
        if (separationLen > 0.f)
          boid.force += (separation / separationLen) * SEPARATION;

        glm::vec2 cohesion = (*boidsCohesion)[i] - boid.pos;
        const float cohesionLen = glm::length(cohesion);
        if (cohesionLen > 0.f)
          boid.force += (cohesion / cohesionLen) * COHESION;

        glm::vec2 alignment = (*boidsAlignment)[i] - boid.vel;
        const float alignmentLen = glm::length(alignment);
        if (alignmentLen > 0.f)
          boid.force += (alignment / alignmentLen) * ALIGNMENT;
      }
    });

    ecs::set_system_job(sid, steerJob);

    jobmanager::start_jobs();
  }
};

#endif // USE_GRID_BOIDS