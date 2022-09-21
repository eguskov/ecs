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

// The way to use any struct as a component
// struct MyComp final : public ExternalLibraryStructAsComponet
// {
//   ECS_COMPONENT_TYPE;
//   ECS_BIND_FIELDS(foo, bar);
// };

MAKE_TYPE_FACTORY(Texture2D, ::Texture2D);
ECS_COMPONENT_TYPE(Texture2D);

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

  ECS_BIND(name=node)
  eastl::string currentNode;

  float startTime = 0.0;
  int frameNo = 0;
  bool done = false;

  ECS_DEFAULT_CTORS(AnimState);
};

ECS_COMPONENT_TYPE_BIND(AnimState);