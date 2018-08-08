#include "ecs/ecs.h"

#include "update.ecs.h"

#include <random>
#include <ctime>
#include <future>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "components/timer.ecs.h"
#include "components/color.ecs.h"

extern int screen_width;
extern int screen_height;

struct Wall DEF_EMPTY_COMP(Wall, wall);
struct Enemy DEF_EMPTY_COMP(Enemy, enemy);

struct Gravity
{
  float mass = 0.f;

  bool set(const JValue &value)
  {
    mass = value["mass"].GetFloat();
    return true;
  };
}
DEF_COMP(Gravity, gravity);

struct UserInput
{
  bool left = false;
  bool right = false;
  bool jump = false;

  bool set(const JValue&) { return true; };
}
DEF_COMP(UserInput, user_input);

struct Jump
{
  bool acitve = false;

  float startTime = 0.f;
  float height = 0.f;
  float duration = 0.f;

  bool set(const JValue &value)
  {
    height = value["height"].GetFloat();
    duration = value["duration"].GetFloat();
    return true;
  };
}
DEF_COMP(Jump, jump);

struct TextureComp
{
  eastl::string path;

  int width = 0;

  Texture2D id;
  Texture2D flippedId;

  bool set(const JValue &value)
  {
    assert(value.HasMember("path"));
    path = value["path"].GetString();
    return true;
  };
}
DEF_COMP(TextureComp, texture);

struct AnimGraph
{
  struct Node
  {
    bool loop = false;
    float fps = 0.f;
    float frameDt = 0.f;
    eastl::vector<glm::vec4> frames;
  };

  eastl::string_map<Node> nodesMap;

  bool set(const JValue &value)
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
}
DEF_COMP(AnimGraph, anim_graph);

struct AnimState
{
  bool done = false;
  int frameNo = 0;
  float startTime = 0.f;
  eastl::string currentNode;

  bool set(const JValue &value)
  {
    currentNode = value["node"].GetString();
    return true;
  }
}
DEF_COMP(AnimState, anim_state);

struct AutoMove
{
  bool set(const JValue &value)
  {
    return true;
  }
}
DEF_COMP(AutoMove, auto_move);

DEF_SYS()
static __forceinline void load_texture_handler(const EventOnEntityCreate &ev, TextureComp &texture)
{
  Image image = LoadImage(texture.path.c_str());
  ImageFormat(&image, UNCOMPRESSED_R8G8B8A8);
  texture.id = LoadTextureFromImage(image);
  ImageFlipHorizontal(&image);
  texture.flippedId = LoadTextureFromImage(image);
  texture.width = texture.id.width;
}

DEF_SYS(IS_TRUE(is_alive))
static __forceinline void update_position(
  const UpdateStage &stage,
  const glm::vec2 &vel,
  glm::vec2 &pos)
{
  pos += vel * stage.dt;
}

DEF_SYS()
static __forceinline void update_anim_frame(
  const UpdateStage &stage,
  const AnimGraph &anim_graph,
  AnimState &anim_state,
  glm::vec4 &frame)
{
  const auto &node = anim_graph.nodesMap.at(anim_state.currentNode.c_str());

  const int frameNo = (int)floorf((stage.total - anim_state.startTime) / node.frameDt);
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

DEF_SYS(HAVE_COMP(wall) IS_TRUE(is_alive))
static __forceinline void render_walls(
  const RenderStage &stage,
  const TextureComp &texture,
  const glm::vec4 &frame,
  const glm::vec2 &pos)
{
  const float hw = screen_width * 0.5f;
  const float hh = screen_height * 0.5f;
  DrawTextureRec(texture.id, Rectangle{ frame.x, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
}


DEF_SYS(NOT_HAVE_COMP(wall) IS_TRUE(is_alive) IS_TRUE(is_flipped))
static __forceinline void render_flipped(
  const RenderStage &stage,
  const TextureComp &texture,
  const glm::vec4 &frame,
  const glm::vec2 &pos)
{
  const float hw = screen_width * 0.5f;
  const float hh = screen_height * 0.5f;
  DrawTextureRec(texture.flippedId, Rectangle{ texture.width - frame.x - frame.z, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  DrawCircleV(Vector2{ hw + pos.x, hh + pos.y }, 2, CLITERAL{ 0, 255, 0, 255 });
}

DEF_SYS(NOT_HAVE_COMP(wall) IS_TRUE(is_alive) IS_FALSE(is_flipped))
static __forceinline void render_normal(
  const RenderStage &stage,
  const TextureComp &texture,
  const glm::vec4 &frame,
  const glm::vec2 &pos)
{
  const float hw = screen_width * 0.5f;
  const float hh = screen_height * 0.5f;
  DrawTextureRec(texture.id, Rectangle{ frame.x, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  DrawCircleV(Vector2{ hw + pos.x, hh + pos.y }, 2, CLITERAL{ 255, 0, 0, 255 });
}

DEF_SYS()
static __forceinline void read_controls(
  const UpdateStage &stage,
  UserInput &user_input,
  glm::vec2 &vel)
{
  user_input = { false, false, false };

  if (IsKeyDown(KEY_LEFT))
    user_input.left = true;
  else if (IsKeyDown(KEY_RIGHT))
    user_input.right = true;
  if (IsKeyReleased(KEY_SPACE))
    user_input.jump = true;
}

DEF_SYS()
static __forceinline void apply_controls(
  const UpdateStage &stage,
  const UserInput &user_input,
  Jump &jump,
  glm::vec2 &vel,
  bool &is_flipped)
{
  if (user_input.left)
  {
    vel.x = -50.f;
    is_flipped = true;
  }
  else if (user_input.right)
  {
    vel.x = 50.f;
    is_flipped = false;
  }
  else
    vel.x = 0.f;

  if (user_input.jump && !jump.acitve)
  {
    jump.acitve = true;
    jump.startTime = stage.total;
  }
}

DEF_SYS()
static __forceinline void apply_jump(
  const UpdateStage &stage,
  Jump &jump,
  glm::vec2 &vel)
{
  if (!jump.acitve)
    return;

  const float k = glm::clamp((stage.total - jump.startTime) / jump.duration, 0.f, 1.f);
  const float v = jump.height / jump.duration;
  vel.y = -v;

  if (k >= 1.f)
    jump.acitve = false;
}

DEF_SYS(IS_TRUE(is_alive))
static __forceinline void apply_gravity(
  const UpdateStage &stage,
  const Gravity &gravity,
  glm::vec2 &vel)
{
  vel.y += gravity.mass * 9.8f * stage.dt;
}

eastl::tuple<bool, glm::vec2, glm::vec2> collision_detection(const glm::vec2 &pos_a, const glm::vec2 &rect_a, const glm::vec2 &pos_b, const glm::vec2 &rect_b)
{
  const float dx = pos_b.x - pos_a.x;
  const float px = 0.5f * (rect_b.x + rect_a.x) - fabsf(dx);
  if (px < 0)
    return eastl::make_tuple(false, glm::vec2(0.f, 0.f), glm::vec2(0.f, 0.f));

  const float dy = pos_b.y - pos_a.y;
  const float py = 0.5f * (rect_b.y + rect_a.y) - fabsf(dy);
  if (py < 0)
    return eastl::make_tuple(false, glm::vec2(0.f, 0.f), glm::vec2(0.f, 0.f));

  glm::vec2 pos(0.f, 0.f);
  glm::vec2 normal(0.f, 0.f);
  if (px < py)
  {
    const float sx = dx > 0.f ? 1.f : dx < 0.f ? -1.f : 0.f;
    normal.x = sx;
    pos.x = pos_a.x + 0.5f * (rect_a.x * sx);
    pos.y = pos_b.y;
  }
  else
  {
    const float sy = dy > 0.f ? 1.f : dy < 0.f ? -1.f : 0.f;
    normal.y = sy;
    pos.x = pos_b.x;
    pos.y = pos_a.y + 0.5f * (rect_a.y * sy);
  }
  return eastl::make_tuple(true, normal, pos);
}

DEF_QUERY(BricksQuery, HAVE_COMP(wall));

DEF_SYS(HAVE_COMP(user_input))
static __forceinline void update_collisions(
  const UpdateStage &stage,
  const EntityId &eid,
  const glm::vec2 &collision_rect,
  glm::vec2 &pos,
  glm::vec2 &vel)
{
  EntityId myEid = eid;
  glm::vec2 myPos = pos;
  glm::vec2 myVel = vel;
  glm::vec2 myCollisionRect = collision_rect;

  BricksQuery::exec([myEid, &myPos, &myVel, myCollisionRect, &stage](const EntityId &eid, const glm::vec2 &collision_rect, const glm::vec2 &pos)
  {
    if (eid == myEid)
      return;

    bool isHit;
    glm::vec2 normal;
    glm::vec2 hitPos;
    eastl::tie(isHit, normal, hitPos) = collision_detection(pos, collision_rect, myPos, myCollisionRect);
    if (isHit)
    {
      float d = 0.f;
      if (normal.y < 0.f)
        d = myCollisionRect.y - (pos.y - myPos.y);
      else if (normal.y > 0.f)
        d = pos.y + collision_rect.y - myPos.y;
      else if (normal.x < 0.f)
        d = myCollisionRect.x - (pos.x - myPos.x);
      else if (normal.x > 0.f)
        d = pos.x + collision_rect.x - myPos.x;
      const float proj = glm::dot(myVel, normal);
      if (proj < 0.f)
        myVel += fabsf(proj) * normal;
      myPos += fabsf(d) * normal;
    }
  });

  pos = myPos;
  vel = myVel;
}

DEF_SYS(HAVE_COMP(auto_move) IS_TRUE(is_alive))
static __forceinline void update_auto_move_collisions(
  const UpdateStage &stage,
  const EntityId &eid,
  const glm::vec2 &collision_rect,
  glm::vec2 &pos,
  glm::vec2 &vel,
  bool &is_flipped)
{
  EntityId myEid = eid;
  glm::vec2 myPos = pos;
  glm::vec2 myVel = vel;
  glm::vec2 myCollisionRect = collision_rect;

  BricksQuery::exec([myEid, &is_flipped, &myPos, &myVel, myCollisionRect, &stage](const EntityId &eid, const glm::vec2 &collision_rect, const glm::vec2 &pos)
  {
    if (eid == myEid)
      return;

    bool isHit;
    glm::vec2 normal;
    glm::vec2 hitPos;
    eastl::tie(isHit, normal, hitPos) = collision_detection(pos, collision_rect, myPos, myCollisionRect);
    if (isHit)
    {
      if (normal.x < 0.f || normal.x > 0.f)
      {
        myVel.x = normal.x * fabsf(myVel.x);
        is_flipped = !is_flipped;
      }
    }
  });

  pos = myPos;
  vel = myVel;
}

static void set_anim_node(AnimState &anim_state, const char *node, float start_time)
{
  if (anim_state.currentNode != node)
  {
    anim_state.frameNo = 0;
    anim_state.startTime = start_time;
  }
  anim_state.currentNode = node;
}

DEF_SYS(HAVE_COMP(user_input))
static __forceinline void select_current_anim_frame(
  const UpdateStage &stage,
  const glm::vec2 &vel,
  TextureComp &texture,
  AnimState &anim_state)
{
  if (vel.x != 0.f)
    set_anim_node(anim_state, "run", stage.total);
  else
    set_anim_node(anim_state, "idle", stage.total);

  if (vel.y < 0.f)
    set_anim_node(anim_state, "jump", stage.total);
  else if (vel.y > 0.f)
    set_anim_node(anim_state, "fall", stage.total);
}

DEF_SYS()
static __forceinline void validate_position(
  const UpdateStage &stage,
  glm::vec2 &pos)
{
  const float hw = 0.5f * screen_width;
  const float hh = 0.5f * screen_width;
  if (pos.x > hw || pos.y > hh || pos.x < -hw || pos.y < -hh)
  {
    pos.x = 0.f;
    pos.y = 0.f;
  }
}

DEF_SYS(HAVE_COMP(auto_move) IS_TRUE(is_alive))
static __forceinline void update_auto_move(
  const UpdateStage &stage,
  const AutoMove &auto_move,
  glm::vec2 &vel)
{
}

DEF_QUERY(AliveEnemiesQuery, HAVE_COMP(enemy) IS_TRUE(is_alive));

DEF_SYS(IS_TRUE(is_alive))
static __forceinline void update_enemies_collisions(
  const UpdateStage &stage,
  const EntityId &eid,
  UserInput &user_input,
  const glm::vec2 &collision_rect,
  glm::vec2 &pos,
  glm::vec2 &vel,
  bool &is_flipped)
{
  EntityId myEid = eid;
  glm::vec2 myPos = pos;
  glm::vec2 myVel = vel;
  glm::vec2 myCollisionRect = collision_rect;

  AliveEnemiesQuery::exec([myEid, &is_flipped, &user_input, &myPos, &myVel, myCollisionRect, &stage](
    const EntityId &eid,
    const glm::vec2 &collision_rect,
    const glm::vec2 &pos,
    glm::vec2 &vel,
    AnimState &anim_state)
  {
    if (eid == myEid)
      return;

    bool isHit;
    glm::vec2 normal;
    glm::vec2 hitPos;
    eastl::tie(isHit, normal, hitPos) = collision_detection(pos, collision_rect, myPos, myCollisionRect);
    if (isHit)
    {
      if (normal.y < 0.f)
      {
        set_anim_node(anim_state, "death", stage.total);

        user_input.jump = true;
        vel = glm::vec2(0.f, 0.f);
      }
    }
  });

  pos = myPos;
  vel = myVel;
}

DEF_SYS(IS_TRUE(is_alive))
static __forceinline void update_death(
  const UpdateStage &stage,
  const AnimState &anim_state,
  bool &is_alive)
{
  if (anim_state.currentNode == "death" && anim_state.done)
    is_alive = false;
}

DEF_SYS(IS_FALSE(is_alive))
static __forceinline void update_spawn_timer(
  const UpdateStage &stage,
  TimerComponent &spawn_timer,
  AnimState &anim_state,
  glm::vec2 &vel,
  bool &is_alive,
  bool &is_flipped)
{
  spawn_timer.time -= stage.dt;
  if (spawn_timer.time < 0.f)
  {
    spawn_timer.time = spawn_timer.period;
    anim_state.frameNo = 0;
    anim_state.currentNode = "run";
    is_alive = true;
    is_flipped = false;
    vel = glm::vec2(-20.f, 0.f);
  }
}