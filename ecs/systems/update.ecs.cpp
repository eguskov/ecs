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

  bool isFlipped = false;

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
      node.frameDt = 1.f / node.fps;
      for (int i = 0; i < it->value["frames"].Size(); ++i)
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
  int frameNo = 0;
  eastl::string currentNode;

  bool set(const JValue &value)
  {
    currentNode = value["node"].GetString();
    return true;
  }
}
DEF_COMP(AnimState, anim_state);

DEF_SYS()
static __forceinline void load_texture_handler(const EventOnEntityCreate &ev, TextureComp &texture)
{
  Image image = LoadImage(texture.path.c_str());
  ImageFormat(&image, UNCOMPRESSED_R8G8B8A8);
  texture.id = LoadTextureFromImage(image);
  ImageFlipHorizontal(&image);
  texture.flippedId = LoadTextureFromImage(image);
  texture.width = texture.id.width;
  // texture.id = LoadTexture(texture.path.c_str());
}

DEF_SYS()
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
  anim_state.frameNo = ((int)floorf(stage.total / node.frameDt)) % node.frames.size();
  frame = node.frames[anim_state.frameNo];
}

DEF_SYS()
static __forceinline void render(
  const RenderStage &stage,
  const TextureComp &texture,
  const glm::vec4 &frame,
  const glm::vec2 &pos)
{
  const float hw = screen_width * 0.5f;
  const float hh = screen_height * 0.5f;
  DrawTextureRec(texture.isFlipped ? texture.flippedId : texture.id, Rectangle{ texture.isFlipped ?  texture.width - frame.x - frame.w : frame.x, frame.y, frame.z, frame.w }, Vector2{ hw + pos.x, hh + pos.y }, WHITE);
  DrawCircleV(Vector2{ hw + pos.x, hh + pos.y }, 2, CLITERAL{ 255, 0, 0, 255 });
}

//DEF_SYS()
//static __forceinline void update_vels(
//  const UpdateStage &stage,
//  const ArrayComp<glm::vec2, 2> &vels)
//{
//  vels[0];
//}

//DEF_SYS()
//static __forceinline void update_velocity(
//  const UpdateStage &stage,
//  float damping,
//  const glm::vec2 &pos,
//  const glm::vec2 &pos_copy,
//  glm::vec2 &vel)
//{
//  const int hw = 10;
//  const int hh = 10;
//  if (pos.x < hw || pos.x > screen_width - hw)
//    vel.x = -vel.x;
//  if (pos.y < hh || pos.y > screen_height - hh)
//    vel.y = -vel.y;
//
//  vel *= 1.0f / (1.0f + stage.dt * damping);
//}

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
  glm::vec2 &vel)
{
  if (user_input.left)
    vel.x = -50.f;
  else if (user_input.right)
    vel.x = 50.f;
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

DEF_SYS()
static __forceinline void apply_gravity(
  const UpdateStage &stage,
  const Gravity &gravity,
  glm::vec2 &vel)
{
  vel.y += gravity.mass * 9.8f * stage.dt;
}

DEF_QUERY(BricksQuery);

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

DEF_SYS()
static __forceinline void update_collisions(
  const UpdateStage &stage,
  const EntityId &eid,
  const UserInput &user_input,
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

DEF_SYS()
static __forceinline void select_current_anim_frame(
  const UpdateStage &stage,
  const UserInput &user_input,
  const glm::vec2 &vel,
  TextureComp &texture,
  AnimState &anim_state)
{
  if (vel.x != 0.f)
  {
    if (anim_state.currentNode != "run")
      anim_state.frameNo = 0;
    anim_state.currentNode = "run";
  }
  else
  {
    if (anim_state.currentNode != "idle")
      anim_state.frameNo = 0;
    anim_state.currentNode = "idle";
  }

  if (vel.y < 0.f)
  {
    if (anim_state.currentNode != "jump")
      anim_state.frameNo = 0;
    anim_state.currentNode = "jump";
  }
  else if (vel.y > 0.f)
  {
    if (anim_state.currentNode != "fall")
      anim_state.frameNo = 0;
    anim_state.currentNode = "fall";
  }

  texture.isFlipped = vel.x < 0.f;
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

//DEF_QUERY(TestQuery);

//DEF_SYS() HAS_COMP(gravity)
//static __forceinline void update_collisions(
//  const UpdateStage &stage,
//  EntityId eid,
//  float mass,
//  const Gravity &gravity,
//  glm::vec2 &pos,
//  glm::vec2 &vel)
//{
//  EntityId curEid = eid;
//  glm::vec2 curPos = pos;
//  glm::vec2 curVel = vel;
//  float curMass = mass;
//
//  TestQuery::exec([curEid, curPos, &curVel, &stage](EntityId eid, float mass, const glm::vec2 &pos, const glm::vec2 &vel)
//  {
//    if (eid == curEid)
//      return;
//    const float r = glm::length(curPos - pos);
//    if (r <= 0.01f || r > 10.f)
//      return;
//    const glm::vec2 f = (pos - curPos) * ((r - 10.f) / 10.f);
//    curVel += f * stage.dt;
//  });
//
//  // vel = curVel;
//}

//DEF_SYS()
//static __forceinline void spawner(const UpdateStage &stage, EntityId eid, TimerComponent &timer)
//{
//  timer.time += stage.dt;
//  if (timer.time >= timer.period)
//  {
//    timer.time = 0.f;
//    // g_mgr->sendEvent(eid, EventOnSpawn{});
//  }
//}

//DEF_SYS()
//static __forceinline void render(
//  const RenderStage &stage,
//  EntityId eid,
//  const ColorComponent &color,
//  const glm::vec2 &pos)
//{
//  DrawCircleV(Vector2{ pos.x, pos.y }, 10, CLITERAL{ color.r, color.g, color.b, 255 });
//}
//
//DEF_SYS()
//static __forceinline void spawn_handler(
//  const EventOnSpawn &ev,
//  EntityId eid,
//  const glm::vec2 &vel,
//  const glm::vec2 &pos)
//{
//  for (int i = 0; i < ev.count; ++i)
//  {
//    JDocument doc;
//    auto &a = doc.GetAllocator();
//
//    const float sx = (float)GetRandomValue(-50, 50);
//    const float sy = (float)GetRandomValue(-50, 50);
//
//    JValue posValue(rapidjson::kObjectType);
//    posValue.AddMember("x", pos.x, a);
//    posValue.AddMember("y", pos.y, a);
//
//    JValue velValue(rapidjson::kObjectType);
//    velValue.AddMember("x", sx, a);
//    velValue.AddMember("y", sy, a);
//
//    JValue colorValue(rapidjson::kObjectType);
//    colorValue.AddMember("r", std::rand() % 256, a);
//    colorValue.AddMember("g", std::rand() % 256, a);
//    colorValue.AddMember("b", std::rand() % 256, a);
//
//    JValue value(rapidjson::kObjectType);
//    value.AddMember("pos", posValue, a);
//    value.AddMember("vel", velValue, a);
//    value.AddMember("color", colorValue, a);
//
//    g_mgr->createEntity("ball", value);
//  }
//}