#pragma once

#include <ecs/event.h>
#include <ecs/component.h>
#include <ecs/system.h>
#include <ecs/ecs-das.h>

#include <raylib.h>

struct EventOnKillEnemy
{
  glm::vec2 pos = { 0.f, 0.f };
  EventOnKillEnemy() = default;
  EventOnKillEnemy(const glm::vec2 &p) : pos(p) {}
};

ECS_EVENT(EventOnKillEnemy);

struct Timer
{
  ECS_BIND_TYPE(sample, isLocal=true);
  ECS_BIND_FIELDS(time, period);

  float time = 0.f;
  float period = 5.f;
};

ECS_COMPONENT_TYPE(Timer);

struct NoHitTimer
{
  ECS_BIND_TYPE(sample, isLocal=true);

  float time = 0.f;
};

ECS_COMPONENT_TYPE(NoHitTimer);

struct ColorComponent
{
  ECS_BIND_TYPE(sample, isLocal=true);
  ECS_BIND_ALL_FIELDS;

  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
};

ECS_COMPONENT_TYPE_ALIAS(ColorComponent, color);

struct AutoMove
{
  ECS_BIND_TYPE(sample, isLocal=true);
  ECS_BIND_FIELDS(jump, duration, length)

  bool jump = false;

  float time = 0.f;
  float duration = 0.f;
  float length = 0.f;
};

ECS_COMPONENT_TYPE(AutoMove);

// The way to use any struct as a component
// struct MyComp final : public ExternalLibraryStructAsComponet
// {
//   ECS_COMPONENT_TYPE;
//   ECS_BIND_FIELDS(foo, bar);
// };

struct Jump
{
  ECS_BIND_TYPE(sample, isLocal=true);

  // TODO: Use ECS_COMPONENT_TYPE; here instead of outside
  // ECS_COMPONENT_TYPE;
  // TODO: Add ECS_BIND_ALL_FIELDS; in order to bind all fields;
  // TODO: ADD ECS_BIND_POD; -> ECS_BIND_TYPE(...POD...); bind all fields;
  ECS_BIND_FIELDS(active, startTime, height, duration);

  bool active = false;

  double startTime = 0.0;
  float height = 0.f;
  float duration = 0.f;
};

// This must stay as a way to craete component's types (POD types)
ECS_COMPONENT_TYPE(Jump);

//  TODO: Do not use components like this. This just a float!
struct Gravity
{
  ECS_BIND_TYPE(sample, isLocal=true);
  ECS_BIND_ALL_FIELDS;

  float mass = 0.f;
};

ECS_COMPONENT_TYPE(Gravity);

struct TextureAtlas
{
  ECS_BIND_TYPE(render, isLocal=true);

  // ECS_BIND(name=path; type=string;)
  // or

  ECS_BIND(name=path)
  eastl::string path;

  Texture2D id;

  ECS_DEFAULT_CTORS(TextureAtlas);
};

ECS_COMPONENT_TYPE(TextureAtlas);

// TODO: Auto bind to daScript
struct AnimGraph
{
  ECS_BIND_TYPE(anim, canNew=true; isRefType=true; isPod=false; isLocal=true; canCopy=true; canMove=true);

  ECS_BIND(sideEffect=modifyArgument; isBuiltin=false)
  static void add_node(AnimGraph *&ag, const char *name, float fps, bool loop, const das::TArray<das::float4> &frames)
  {
    ASSERT(sizeof(glm::vec4) == sizeof(das::float4));

    AnimGraph::Node node;
    node.fps = fps;
    node.loop = loop;
    node.frames.resize(frames.size);
    node.frameDt = 1.f / node.fps;
    ::memcpy(node.frames.data(), frames.data, sizeof(das::float4) * frames.size);

    ag->nodesMap[name] = eastl::move(node);
  }

  struct Node
  {
    bool loop = false;
    float fps = 0.f;
    float frameDt = 0.f;
    eastl::vector<glm::vec4> frames;
  };

  eastl::hash_map<eastl::string, Node> nodesMap;

  ECS_DEFAULT_CTORS(AnimGraph);
};

ECS_COMPONENT_TYPE_BIND(AnimGraph);

struct AnimState
{
  ECS_BIND_TYPE(anim, isLocal=true);

  bool done = false;
  int frameNo = 0;
  double startTime = 0.0;

  ECS_BIND(name=node)
  eastl::string currentNode;

  ECS_DEFAULT_CTORS(AnimState);
};

ECS_COMPONENT_TYPE(AnimState);