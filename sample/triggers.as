[query { "$have": "lift", "$is-false": "is_active" }]
class InactiveLift
{
  const HashedString@ key;
  boolean@ is_active;
}

[query { "$have": "switch_trigger" }]
class SwitchTrigger
{
  const vec2@ pos;
}

[query { "$have": "switch_trigger", "$is-false": "is_binded" }]
class NotBindedSwitchTrigger
{
  const HashedString@ key;
  const HashedString@ action_key;
  EntityId@ action_eid;
  boolean@ is_binded;
}

[query { "$have": "switch_trigger", "$is-true": "is_active" }]
class ActiveSwitchTrigger
{
  const EntityId@ action_eid;
  const HashedString@ key;
  const vec2@ pos;
}

[query { "$have": "switch_trigger", "$is-false": "is_active" }]
class InactiveSwitchTrigger
{
  const HashedString@ key;
  const vec2@ pos;
  boolean@ is_active;
}

[query { "$have": "enable_lift_action" }]
class EnableLiftAction
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

[query { "$have": "enable_lift_action", "$is-true": "is_active" }]
class ActiveEnableLiftAction
{
  const EntityId@ eid;
  const HashedString@ key;
}

[system { "$join": [ "NotBindedSwitchTrigger.action_key", "EnableLiftAction.key" ] }]
void bind_trigger_to_enable_lift_action(const UpdateStage@ stage, NotBindedSwitchTrigger@ trigger, const EnableLiftAction@ action)
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

[system { "$join": [ "ActiveSwitchTrigger.action_eid", "InactiveEnableLiftAction.eid" ] }]
void update_active_switch_triggers(const UpdateStage@ stage, const ActiveSwitchTrigger@ trigger, InactiveEnableLiftAction@ action)
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