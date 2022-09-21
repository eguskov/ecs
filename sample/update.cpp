#include <ecs/ecs.h>

#include "update.h"

#include <EASTL/shared_ptr.h>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <raylib.h>

#include <Box2D/Box2D.h>

extern Camera2D camera;
extern int screen_width;
extern int screen_height;

struct HUD
{
  int killCount = 0;
};

ECS_COMPONENT_TYPE(HUD);

struct UserInput
{
  ECS_BIND_TYPE(sample, isLocal=true);
  ECS_BIND_ALL_FIELDS;

  bool left = false;
  bool right = false;
  bool jump = false;
};

ECS_COMPONENT_TYPE_BIND(UserInput);

// TODO: Implement texture manager
static eastl::hash_map<eastl::string, eastl::shared_ptr<Texture2D>> texture_map;
void clear_textures()
{
  for (auto &t : texture_map)
    UnloadTexture(*t.second);
  texture_map.clear();
}

struct load_texture_handler
{
  ECS_RUN(const EventOnEntityCreate &ev, const eastl::string &texture_path, Texture2D &texture_id)
  {
    auto it = texture_map.find(texture_path);
    if (it == texture_map.end())
    {
      auto id = eastl::make_shared<Texture2D>(LoadTexture(texture_path.c_str()));
      texture_id = *id.get();
      texture_map[texture_path] = id;
    }
    else
      texture_id = *it->second.get();
  }
};

struct update_position
{
  ECS_AFTER(after_phys_update);
  ECS_BEFORE(before_render);

  QL_NOT_HAVE(is_active);
  QL_WHERE(is_alive == true);

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * evt.dt;
  }

  // TODO: Will share QL_* filters but not componets requirements
  // ECS_ON(const EventOnSomething &ev)
  // {

  // }
};

struct update_position_for_active
{
  ECS_AFTER(after_phys_update);
  ECS_BEFORE(update_position);

  QL_WHERE(is_alive == true && is_active == true);

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * evt.dt;
  }
};

struct update_anim_frame
{
  ECS_AFTER(before_anim_update);
  ECS_BEFORE(after_anim_update);

  ECS_RUN(const EventUpdate &evt, const AnimGraph &anim_graph, AnimState &anim_state, glm::vec4 &frame)
  {
    if (anim_state.startTime <= 0.0)
      anim_state.startTime = evt.total;

    auto res = anim_graph.nodesMap.find_as(anim_state.currentNode.c_str());
    if (res == anim_graph.nodesMap.end())
      res = anim_graph.nodesMap.begin();

    const auto &node = res->second;

    const int frameNo = (int)floor((evt.total - anim_state.startTime) / (double)node.frameDt);
    if (node.loop)
    {
      anim_state.frameNo = frameNo % node.frames.size();
      anim_state.done = false;
    }
    else
    {
      anim_state.frameNo = frameNo < (int)node.frames.size() ? frameNo : ((int)node.frames.size() - 1);
      anim_state.done = anim_state.frameNo >= (int)node.frames.size() - 1;
    }
    frame = node.frames[anim_state.frameNo];
  }
};

struct render_walls
{
  ECS_AFTER(before_render);
  ECS_BEFORE(after_render);

  QL_HAVE(wall);
  QL_WHERE(is_alive == true);

  ECS_RUN(const EventRender &evt, const Texture2D &texture_id, const glm::vec4 &frame, const glm::vec2 &pos)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    DrawTextureRec(texture_id, Rectangle{ frame.x, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  }
};

struct render_normal
{
  ECS_AFTER(before_render);
  ECS_BEFORE(after_render);

  QL_NOT_HAVE(wall);
  QL_WHERE(is_alive == true);

  ECS_RUN(const EventRender &evt, const Texture2D &texture_id, const glm::vec4 &frame, const glm::vec2 &pos, float dir)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    DrawTextureRec(texture_id, Rectangle{ frame.x, frame.y, dir * frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  }
};

struct read_controls
{
  ECS_AFTER(before_input);
  ECS_BEFORE(after_input);

  ECS_RUN(const EventUpdate &evt, UserInput &user_input)
  {
    user_input = {};

    if (IsKeyDown(KEY_LEFT))
      user_input.left = true;
    else if (IsKeyDown(KEY_RIGHT))
      user_input.right = true;
    if (IsKeyPressed(KEY_SPACE))
      user_input.jump = true;
  }
};

static void set_anim_node(const AnimGraph &anim_graph, AnimState &anim_state, const char *node, double start_time)
{
  if (anim_graph.nodesMap.find_as(node) == anim_graph.nodesMap.end())
    return;

  if (anim_state.currentNode != node)
  {
    anim_state.frameNo = 0;
    anim_state.startTime = start_time;
  }
  anim_state.currentNode = node;
}

struct select_current_anim_frame
{
  ECS_AFTER(before_anim_update);
  ECS_BEFORE(update_anim_frame);

  QL_NOT_HAVE(user_input);

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, const AnimGraph &anim_graph, bool is_on_ground, AnimState &anim_state)
  {
    if (vel.x != 0.f)
      set_anim_node(anim_graph, anim_state, "run", evt.total);
    else
      set_anim_node(anim_graph, anim_state, "idle", evt.total);

    if (vel.y < 0.f)
      set_anim_node(anim_graph, anim_state, "jump", evt.total);
    else if (vel.y > 0.f && !is_on_ground)
      set_anim_node(anim_graph, anim_state, "fall", evt.total);
  }
};

struct select_current_anim_frame_for_player
{
  ECS_AFTER(before_anim_update);
  ECS_BEFORE(update_anim_frame);

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &vel, const AnimGraph &anim_graph, const UserInput &user_input, bool is_on_ground, AnimState &anim_state)
  {
    if (vel.x != 0.f && (user_input.left || user_input.right))
      set_anim_node(anim_graph, anim_state, "run", evt.total);
    else
      set_anim_node(anim_graph, anim_state, "idle", evt.total);

    if (vel.y < 0.f)
      set_anim_node(anim_graph, anim_state, "jump", evt.total);
    else if (vel.y > 0.f && !is_on_ground)
      set_anim_node(anim_graph, anim_state, "fall", evt.total);
  }
};

struct remove_death_fx
{
  QL_HAVE(death_fx);

  QL_WHERE(is_alive == true);

  ECS_RUN(const EventUpdate &evt, EntityId eid, const AnimState &anim_state, bool &is_alive)
  {
    if (anim_state.done)
    {
      is_alive = false;
      ecs::delete_entity(eid);
    }
  }
};

struct update_camera
{
  ECS_AFTER(camera_update);
  ECS_BEFORE(before_render);

  QL_HAVE(user_input);

  ECS_RUN(const EventUpdate &evt, const glm::vec2 &pos)
  {
    const float hw = 0.5f * screen_width;
    const float hh = 0.5f * screen_width;
    camera.target = Vector2{ pos.x * camera.zoom, pos.y * camera.zoom };
  }
};

struct process_on_kill_event
{
  ECS_RUN(const EventOnKillEnemy &ev, HUD &hud)
  {
    ++hud.killCount;
  }
};

static __forceinline void update_auto_move_impl(
  const EventUpdate &evt,
  const bool auto_move_jump,
  const float auto_move_duration,
  const float auto_move_length,
  float &auto_move_time,
  glm::vec2 &vel,
  float &dir)
{
  if (!auto_move_jump && auto_move_length > 0.f && auto_move_duration > 0.f)
  {
    if (glm::length(vel) > 0.f)
      vel = (auto_move_length / auto_move_duration) * glm::normalize(vel);

    auto_move_time -= evt.dt;
    if (auto_move_time < 0.f)
    {
      auto_move_time = auto_move_duration;
      vel = -vel;
    }
  }

  if (vel.x < 0.f)
    dir = 1.f;
  else if (vel.x > 0.f)
    dir = -1.f;
}

// TODO: Add Optional<bool, true> is_active
struct update_active_auto_move
{
  ECS_AFTER(after_input);
  ECS_BEFORE(collisions_update);

  QL_WHERE(is_alive == true && is_active == true);

  ECS_RUN(const EventUpdate &evt, const bool auto_move_jump, const float auto_move_duration, const float auto_move_length, float &auto_move_time, glm::vec2 &vel, float &dir)
  {
    update_auto_move_impl(evt, auto_move_jump, auto_move_duration, auto_move_length, auto_move_time, vel, dir);
  }
};

// TODO: Add Optional<bool, true> is_active
struct update_always_active_auto_move
{
  ECS_AFTER(after_input);
  ECS_BEFORE(collisions_update);

  QL_NOT_HAVE(is_active);
  QL_WHERE(is_alive == true);

  ECS_RUN(const EventUpdate &evt, const bool auto_move_jump, const float auto_move_duration, const float auto_move_length, float &auto_move_time, glm::vec2 &vel, float &dir)
  {
    update_auto_move_impl(evt, auto_move_jump, auto_move_duration, auto_move_length, auto_move_time, vel, dir);
  }
};

struct on_enenmy_kill_handler
{
  QL_HAVE(user_input);

  ECS_RUN(const EventOnKillEnemy &ev)
  {
    ComponentsMap cmap;
    cmap.add(HASH("pos"), ev.pos);
    ecs::create_entity("death_fx", eastl::move(cmap));
  }
};

struct update_auto_jump
{
  ECS_AFTER(after_input);
  ECS_BEFORE(collisions_update);

  QL_WHERE(is_alive == true);

  ECS_RUN(const EventUpdate &evt,
          bool is_alive,
          bool &jump_active,
          double &jump_startTime,
          const bool auto_move_jump,
          const float auto_move_duration,
          float &auto_move_time,
          glm::vec2 &vel,
          float &dir)
  {
    if (!auto_move_jump)
      return;

    auto_move_time -= evt.dt;
    if (auto_move_time < 0.f)
    {
      auto_move_time = auto_move_duration;

      jump_active = true;
      jump_startTime = evt.total;

      vel.x = 40.f * -dir;
      vel.y = -40.f;
    }
  }
};

struct test_empty
{
  ECS_BEFORE(after_input);

  ECS_RUN(const EventUpdate &evt)
  {
  }
};

// TODO: Move to separate file

// input
// update
// integrate
// solve collisions
// tick physics
// update animation
// render

struct before_input
{
  ECS_BARRIER;
  ECS_BEFORE(after_input);
};

struct after_input
{
  ECS_BARRIER;
  ECS_AFTER(before_input);
};

struct collisions_update
{
  ECS_BARRIER;
  ECS_AFTER(after_input);
};

struct before_phys_update
{
  ECS_BARRIER;
  ECS_AFTER(collisions_update);
};

struct after_phys_update
{
  ECS_BARRIER;
  ECS_AFTER(before_phys_update);
};

struct before_anim_update
{
  ECS_BARRIER;
  ECS_AFTER(after_phys_update);
};

struct after_anim_update
{
  ECS_BARRIER;
  ECS_AFTER(before_anim_update);
};

struct camera_update
{
  ECS_BARRIER;
  ECS_AFTER(after_anim_update);
};

struct before_render
{
  ECS_BARRIER;
  ECS_AFTER(camera_update);
};

struct after_render
{
  ECS_BARRIER;
  ECS_AFTER(before_render);
};