#include <ecs/ecs.h>

#include <stages/update.stage.h>

struct InactiveLift
{
  ECS_QUERY;

  QL_HAVE(lift);
  QL_WHERE(is_active == false);

  const int &key;
  bool &is_active;
};

struct CageBlock
{
  ECS_QUERY;

  QL_HAVE(cage);

  const EntityId &eid;
};

struct NotBindedTrigger
{
  ECS_QUERY;

  QL_HAVE(trigger);
  QL_WHERE(is_binded == false);

  const EntityId &eid;
  const int &key;
  const int &action_key;
  EntityId &action_eid;
  bool &is_binded;
};

struct ActiveTrigger
{
  ECS_QUERY;

  QL_HAVE(trigger);
  QL_WHERE(is_active == true);

  const EntityId &eid;
  const EntityId &action_eid;
  const int &key;
  const glm::vec2 &pos;
  bool &is_active;
};

struct InactiveSwitchTrigger
{
  ECS_QUERY;

  QL_HAVE(switch_trigger);
  QL_WHERE(is_active == false);

  const int &key;
  const glm::vec2 &pos;
  bool &is_active;
};

struct InactiveZoneTrigger
{
  ECS_QUERY;

  QL_HAVE(zone_trigger);
  QL_WHERE(is_active == false);

  const int &key;
  const glm::vec2 &pos;
  const glm::vec4 &collision_rect;
  bool &is_active;
};

struct Action
{
  ECS_QUERY;

  QL_HAVE(action);

  const EntityId &eid;
  const int &key;
};

struct InactiveEnableLiftAction
{
  ECS_QUERY;

  QL_HAVE(enable_lift_action);
  QL_WHERE(is_active == false);

  const EntityId &eid;
  const int &key;
  const int &lift_key;
  bool &is_active;
};

struct InactiveOpenCageAction
{
  ECS_QUERY;

  QL_HAVE(open_cage_action);
  QL_WHERE(is_active == false);

  const EntityId &eid;
  const int &key;
  bool &is_active;
};

struct InactiveKillPlayerAction
{
  ECS_QUERY;

  QL_HAVE(kill_player_action);
  QL_WHERE(is_active == false);

  const EntityId &eid;
  const int &key;
  bool &is_active;
};

struct AlivePlayer
{
  ECS_QUERY;

  QL_HAVE(user_input);
  QL_WHERE(is_alive == true);

  const EntityId &eid;
  const glm::vec2 &pos;
};

struct PlayerSpawnZone
{
  ECS_QUERY;

  QL_HAVE(player_spawn_zone);

  const glm::vec2 &pos;
};

struct update_player_spawner
{
  QL_HAVE(player_spawner);

  ECS_RUN(const UpdateStage &stage)
  {
    if (AlivePlayer::count() != 0)
      return;

    bool isSpawnPosValid = false;
    glm::vec2 spawnPos;
    PlayerSpawnZone::foreach([&](PlayerSpawnZone &&zone)
    {
      isSpawnPosValid = true;
      spawnPos = zone.pos;
    });

    if (!isSpawnPosValid)
      return;

    DEBUG_LOG("Spawn player");

    ComponentsMap cmap;
    cmap.add(HASH("pos"), spawnPos);
    cmap.add(HASH("vel"), glm::vec2());
    ecs::create_entity("fox", eastl::move(cmap));
  }
};

struct bind_trigger_to_action
{
  QL_INDEX(NotBindedTrigger action_key);
  QL_INDEX_LOOKUP(action.key);

  ECS_RUN(const UpdateStage &stage, Action &&action, NotBindedTrigger &&trigger)
  {
    ASSERT(!(action.eid == trigger.eid)); // Check for entity collision
    ASSERT(action.key == trigger.action_key);
    ASSERT(!trigger.is_binded);

    trigger.is_binded = true;
    trigger.action_eid = action.eid;
  }
};

struct update_inactive_switch_triggers
{
  ECS_RUN(const UpdateStage &stage, AlivePlayer &&player, InactiveSwitchTrigger &&trigger)
  {
    if (glm::distance(player.pos, trigger.pos) < 1.f)
    {
      // print("*** Activate SwitchTrigger: "+trigger.key.str());
      trigger.is_active = true;
    }
  }
};

struct update_inactive_zone_triggers
{
  ECS_RUN(const UpdateStage &stage, AlivePlayer &&player, InactiveZoneTrigger &&trigger)
  {
    if (player.pos.x >= trigger.pos.x &&
        player.pos.y >= trigger.pos.y &&
        player.pos.x <= trigger.pos.x + trigger.collision_rect.z &&
        player.pos.y <= trigger.pos.y + trigger.collision_rect.w)
    {
      // print("*** Activate ZoneTrigger: "+trigger.key.str());
      trigger.is_active = true;
    }
  }
};

struct update_active_switch_triggers
{
  QL_INDEX(ActiveTrigger action_eid);
  QL_INDEX_LOOKUP(action.eid);

  ECS_RUN(const UpdateStage &stage, InactiveEnableLiftAction &&action, ActiveTrigger &&trigger)
  {
    ASSERT(!(action.eid == trigger.eid)); // Check for entity collision
    ASSERT(action.eid == trigger.action_eid);

    // print("*** Activate EnableLiftAction: "+trigger.key.str()+" => "+action.key.str());
    action.is_active = true;

    InactiveLift::foreach([&](InactiveLift &&lift)
    {
      if (lift.key == action.lift_key)
      {
        lift.is_active = true;
        // print("*** Action lift: "+lift.key.str());
      }
    });
  }
};

struct update_active_open_cage_triggers
{
  QL_INDEX(ActiveTrigger action_eid);
  QL_INDEX_LOOKUP(action.eid);

  ECS_RUN(const UpdateStage &stage, InactiveOpenCageAction &&action, ActiveTrigger &&trigger)
  {
    ASSERT(!(action.eid == trigger.eid)); // Check for entity collision
    ASSERT(action.eid == trigger.action_eid);

    // print("*** Activate EnableLiftAction: "+trigger.key.str()+" => "+action.key.str());
    action.is_active = true;

    CageBlock::foreach([&](CageBlock &&cage)
    {
      ecs::delete_entity(cage.eid);
    });
  }
};

struct update_active_zone_triggers
{
  QL_INDEX(ActiveTrigger action_eid);
  QL_INDEX_LOOKUP(action.eid);

  ECS_RUN(const UpdateStage &stage, InactiveKillPlayerAction &&action, ActiveTrigger &&trigger)
  {
    ASSERT(!(action.eid == trigger.eid)); // Check for entity collision
    ASSERT(action.eid == trigger.action_eid);

    // print("*** Activate KillPlayerAction: "+trigger.key.str()+" => "+action.key.str());
    trigger.is_active = false;

    AlivePlayer::foreach([&](AlivePlayer &&player)
    {
      ecs::delete_entity(player.eid);
    });
  }
};