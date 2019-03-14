[query { "$have": "lift", "$is-false": "is_active" }]
class InactiveLift
{
  const HashedString@ key;
  boolean@ is_active;
}

[query { "$have": "trigger", "$is-false": "is_binded" }]
class NotBindedTrigger
{
  const HashedString@ key;
  const HashedString@ action_key;
  EntityId@ action_eid;
  boolean@ is_binded;
}

[query { "$have": "trigger", "$is-true": "is_active" }]
class ActiveTrigger
{
  const EntityId@ action_eid;
  const HashedString@ key;
  const vec2@ pos;
  boolean@ is_active;
}

[query { "$have": "switch_trigger", "$is-false": "is_active" }]
class InactiveSwitchTrigger
{
  const HashedString@ key;
  const vec2@ pos;
  boolean@ is_active;
}

[query { "$have": "zone_trigger", "$is-false": "is_active" }]
class InactiveZoneTrigger
{
  const HashedString@ key;
  const vec2@ pos;
  const vec4@ collision_rect;
  boolean@ is_active;
}

[query { "$have": "action" }]
class Action
{
  const EntityId@ eid;
  const HashedString@ key;
}

[query { "$have": "enable_lift_action", "$is-false": "is_active" }]
class InactiveEnableLiftAction
{
  const EntityId@ eid;
  const HashedString@ key;
  const HashedString@ lift_key;
  boolean@ is_active;
}

[query { "$have": "kill_player_action", "$is-false": "is_active" }]
class InactiveKillPlayerAction
{
  const EntityId@ eid;
  const HashedString@ key;
  boolean@ is_active;
}

[system { "$join": [ "NotBindedTrigger.action_key", "Action.key" ] }]
void bind_trigger_to_action(const UpdateStage@ stage, NotBindedTrigger@ trigger, const Action@ action)
{
  trigger.is_binded = true;
  trigger.action_eid = action.eid;

  print("*** Action binded: "+trigger.key.str()+" => "+action.key.str());
}

[system]
void update_inactive_switch_triggers(const UpdateStage@ stage, const AlivePlayer@ player, InactiveSwitchTrigger@ trigger)
{
  if (length(player.pos - trigger.pos) < 1.f)
  {
    print("*** Activate SwitchTrigger: "+trigger.key.str());
    trigger.is_active = true;
  }
}

[system]
void update_inactive_zone_triggers(const UpdateStage@ stage, const AlivePlayer@ player, InactiveZoneTrigger@ trigger)
{
  if (player.pos.x >= trigger.pos.x &&
    player.pos.y >= trigger.pos.y &&
    player.pos.x <= trigger.pos.x + trigger.collision_rect.z &&
    player.pos.y <= trigger.pos.y + trigger.collision_rect.w)
  {
    print("*** Activate ZoneTrigger: "+trigger.key.str());
    trigger.is_active = true;
  }
}

[system { "$join": [ "ActiveTrigger.action_eid", "InactiveEnableLiftAction.eid" ] }]
void update_active_switch_triggers(const UpdateStage@ stage, const ActiveTrigger@ trigger, InactiveEnableLiftAction@ action)
{
  print("*** Activate EnableLiftAction: "+trigger.key.str()+" => "+action.key.str());
  action.is_active = true;

  for (auto it = Query<InactiveLift>().perform(); it.hasNext(); ++it)
  {
    InactiveLift@ lift = it.get();
    if (lift.key.hash == action.lift_key.hash)
    {
      lift.is_active = true;
      print("*** Action lift: "+lift.key.str());
    }
  }
}

[system { "$join": [ "ActiveTrigger.action_eid", "InactiveKillPlayerAction.eid" ] }]
void update_active_zone_triggers(const UpdateStage@ stage, ActiveTrigger@ trigger, InactiveKillPlayerAction@ action)
{
  print("*** Activate KillPlayerAction: "+trigger.key.str()+" => "+action.key.str());
  trigger.is_active = false;

  for (auto it = Query<AlivePlayer>().perform(); it.hasNext(); ++it)
  {
    AlivePlayer@ player = it.get();
    delete_entity(player.eid);
  }
}