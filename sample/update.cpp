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
  bool left = false;
  bool right = false;
  bool jump = false;

  bool set(const JFrameValue&) { return true; };
};

ECS_COMPONENT_TYPE(UserInput);

struct AnimGraph
{
  struct Node
  {
    bool loop = false;
    float fps = 0.f;
    float frameDt = 0.f;
    eastl::vector<glm::vec4> frames;
  };

  eastl::hash_map<eastl::string, Node> nodesMap;

  void operator=(const AnimGraph&) { ASSERT(false); }

  bool set(const JFrameValue &value)
  {
    for (auto it = value["nodes"].MemberBegin(); it != value["nodes"].MemberEnd(); ++it)
    {
      auto &node = nodesMap[it->name.GetString()];
      node.fps = it->value["fps"].GetFloat();
      node.loop = it->value["loop"].GetBool();
      node.frameDt = 1.f / node.fps;
      for (int i = 0; i < (int)it->value["frames"].Size(); ++i)
      {
        auto &frame = node.frames.emplace_back();
        frame.x = it->value["frames"][i]["x"].GetFloat();
        frame.y = it->value["frames"][i]["y"].GetFloat();
        frame.z = it->value["frames"][i]["w"].GetFloat();
        frame.w = it->value["frames"][i]["h"].GetFloat();
      }
    }
    return true;
  }
};

ECS_COMPONENT_TYPE(AnimGraph);

struct AnimState
{
  bool done = false;
  int frameNo = 0;
  double startTime = -1.0;
  eastl::string currentNode;

  void operator=(const AnimState&) { ASSERT(false); }

  bool set(const JFrameValue &value)
  {
    currentNode = value["node"].GetString();
    return true;
  }
};

ECS_COMPONENT_TYPE(AnimState);

TextureAtlas::TextureAtlas(TextureAtlas &&assign)
{
  path = eastl::move(assign.path);
  id = assign.id;
  assign.id = Texture2D {};
}

void TextureAtlas::operator=(TextureAtlas &&assign)
{
  path = eastl::move(assign.path);
  id = assign.id;
  assign.id = Texture2D {};
}

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
  ECS_RUN(const EventOnEntityCreate &ev, TextureAtlas &texture)
  {
    auto it = texture_map.find(texture.path);
    if (it == texture_map.end())
    {
      auto id = eastl::make_shared<Texture2D>(LoadTexture(texture.path.c_str()));
      texture.id = *id.get();
      texture_map[texture.path] = id;
    }
    else
      texture.id = *it->second.get();
  }
};

struct update_position
{
  QL_NOT_HAVE(is_active);
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * stage.dt;
  }
};

struct update_position_for_active
{
  QL_WHERE(is_alive == true && is_active == true);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, glm::vec2 &pos)
  {
    pos += vel * stage.dt;
  }
};

struct update_anim_frame
{
  ECS_RUN(const UpdateStage &stage, const AnimGraph &anim_graph, AnimState &anim_state, glm::vec4 &frame)
  {
    if (anim_state.startTime < 0.0)
      anim_state.startTime = stage.total;

    auto res = anim_graph.nodesMap.find_as(anim_state.currentNode.c_str());
    if (res == anim_graph.nodesMap.end())
      res = anim_graph.nodesMap.begin();

    const auto &node = res->second;

    const int frameNo = (int)floor((stage.total - anim_state.startTime) / (double)node.frameDt);
    if (node.loop)
    {
      anim_state.frameNo = frameNo % node.frames.size();
      anim_state.done = false;
    }
    else
    {
      anim_state.frameNo = frameNo < (int)node.frames.size() ? frameNo : node.frames.size() - 1;
      anim_state.done = anim_state.frameNo >= (int)node.frames.size() - 1;
    }
    frame = node.frames[anim_state.frameNo];
  }
};

struct render_walls
{
  QL_HAVE(wall);
  QL_WHERE(is_alive == true);

  ECS_RUN(const RenderStage &stage, const TextureAtlas &texture, const glm::vec4 &frame, const glm::vec2 &pos)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    DrawTextureRec(texture.id, Rectangle{ frame.x, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  }
};

struct render_normal
{
  QL_NOT_HAVE(wall);
  QL_WHERE(is_alive == true);

  ECS_RUN(const RenderStage &stage, const TextureAtlas &texture, const glm::vec4 &frame, const glm::vec2 &pos, float dir)
  {
    const float hw = screen_width * 0.5f;
    const float hh = screen_height * 0.5f;
    DrawTextureRec(texture.id, Rectangle{ frame.x, frame.y, dir * frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  }
};

struct read_controls
{
  ECS_RUN(const UpdateStage &stage, UserInput &user_input)
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

struct apply_controls
{
  ECS_RUN(const UpdateStage &stage, const UserInput &user_input, bool is_on_ground, Jump &jump, glm::vec2 &vel, float &dir)
  {
    if (user_input.left)
    {
      vel.x = -50.f;
      dir = -1.f;
    }
    else if (user_input.right)
    {
      vel.x = 50.f;
      dir = 1.f;
    }
    else
      vel.x = 0.f;

    if (user_input.jump && !jump.active && is_on_ground)
    {
      jump.active = true;
      jump.startTime = stage.total;
    }
  }
};


struct apply_jump
{
  ECS_RUN(const UpdateStage &stage, Jump &jump, bool &is_on_ground, glm::vec2 &vel)
  {
    // TODO: Move jump.active to component
    if (!jump.active)
      return;

    is_on_ground = false;

    const float k = (float)glm::clamp((stage.total - jump.startTime) / (double)jump.duration, 0.0, 1.0);
    const float v = jump.height / jump.duration;
    vel.y = -v;

    if (k >= 1.f)
    {
      vel.y *= 0.5f;
      jump.active = false;
    }
  }
};

struct apply_gravity
{
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, const Gravity &gravity, glm::vec2 &vel)
  {
    vel.y += gravity.mass * 9.8f * stage.dt;
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
  QL_NOT_HAVE(user_input);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, const AnimGraph &anim_graph, bool is_on_ground, TextureAtlas &texture, AnimState &anim_state)
  {
    if (vel.x != 0.f)
      set_anim_node(anim_graph, anim_state, "run", stage.total);
    else
      set_anim_node(anim_graph, anim_state, "idle", stage.total);

    if (vel.y < 0.f)
      set_anim_node(anim_graph, anim_state, "jump", stage.total);
    else if (vel.y > 0.f && !is_on_ground)
      set_anim_node(anim_graph, anim_state, "fall", stage.total);
  }
};

struct select_current_anim_frame_for_player
{
  ECS_RUN(const UpdateStage &stage, const glm::vec2 &vel, const AnimGraph &anim_graph, const UserInput &user_input, bool is_on_ground, TextureAtlas &texture, AnimState &anim_state)
  {
    if (vel.x != 0.f && (user_input.left || user_input.right))
      set_anim_node(anim_graph, anim_state, "run", stage.total);
    else
      set_anim_node(anim_graph, anim_state, "idle", stage.total);

    if (vel.y < 0.f)
      set_anim_node(anim_graph, anim_state, "jump", stage.total);
    else if (vel.y > 0.f && !is_on_ground)
      set_anim_node(anim_graph, anim_state, "fall", stage.total);
  }
};

struct remove_death_fx
{
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, EntityId eid, const AnimState &anim_state, bool &is_alive)
  {
    if (anim_state.currentNode == "death" && anim_state.done)
    {
      is_alive = false;
      ecs::delete_entity(eid);
    }
  }
};

struct update_camera
{
  QL_HAVE(user_input);

  ECS_RUN(const UpdateStage &stage, const glm::vec2 &pos)
  {
    const float hw = 0.5f * screen_width;
    const float hh = 0.5f * screen_width;
    camera.target = Vector2{ hw, hh };
    camera.offset = Vector2{ -pos.x * camera.zoom, -pos.y * camera.zoom };
  }
};

struct process_on_kill_event
{
  ECS_RUN(const EventOnKillEnemy &ev, HUD &hud)
  {
    ++hud.killCount;
  }
};

static __forceinline void update_auto_move_impl(const UpdateStage &stage, AutoMove &auto_move, glm::vec2 &vel, float &dir)
{
  if (!auto_move.jump && auto_move.length > 0.f && auto_move.duration > 0.f)
  {
    if (glm::length(vel) > 0.f)
      vel = (auto_move.length / auto_move.duration) * glm::normalize(vel);

    auto_move.time -= stage.dt;
    if (auto_move.time < 0.f)
    {
      auto_move.time = auto_move.duration;
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
  QL_WHERE(is_alive == true && is_active == true);

  ECS_RUN(const UpdateStage &stage, AutoMove &auto_move, glm::vec2 &vel, float &dir)
  {
    update_auto_move_impl(stage, auto_move, vel, dir);
  }
};

// TODO: Add Optional<bool, true> is_active
struct update_always_active_auto_move
{
  QL_NOT_HAVE(is_active);
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, AutoMove &auto_move, glm::vec2 &vel, float &dir)
  {
    update_auto_move_impl(stage, auto_move, vel, dir);
  }
};

struct on_enenmy_kill_handler
{
  QL_HAVE(enemy);

  ECS_RUN(const EventOnKillEnemy &ev)
  {
    JFrameAllocator alloc;
    JFrameValue comps(rapidjson::kObjectType);
    {
      JFrameValue arr(rapidjson::kArrayType);
      arr.PushBack(ev.pos.x, alloc);
      arr.PushBack(ev.pos.y, alloc);
      comps.AddMember("pos", eastl::move(arr), alloc);
    }
    ecs::create_entity("death_fx", comps);
  }
};

struct update_auto_jump
{
  QL_WHERE(is_alive == true);

  ECS_RUN(const UpdateStage &stage, bool is_alive, Jump &jump, AutoMove &auto_move, glm::vec2 &vel, float &dir)
  {
    // TODO: auto_move.jump must be component
    if (auto_move.jump)
    {
      auto_move.time -= stage.dt;
      if (auto_move.time < 0.f)
      {
        auto_move.time = auto_move.duration;

        jump.active = true;
        jump.startTime = stage.total;

        vel.x = 40.f * -dir;
        vel.y = -40.f;
      }
    }
  }
};