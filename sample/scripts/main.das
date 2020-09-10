require ecs
require sample

// TODO:
// 
// def is_alive_player(is_alive: bool)
//   return is_alive

// [query (have=user_input, filter=is_alive_player)]
// struct AlivePlayer
//   eid: EntityId
//   pos: float2

// TODO: Macros to do this easily
// [es]
// def foreach_alive_player(evt: Event; player: AlivePlayer)
//   ... code ...
// 
// This call must be transformed to 
// [es]
// def __foreach_alive_player__(evt: Event)
//   query() <| $[es (have=user_input)](/* From struct */ eid: EntityId; pos: float2; /* From filter function */ is_alive: bool)
//     if filter(is_alive)
//       foreach_alive_player(evt, [[AlivePlayer eid=eid, pos=pos]])

struct AlivePlayer
  eid: EntityId
  pos: float2

def foreach_alive_player(blk: block<(AlivePlayer):void>)
  query() <| $[es (have=user_input)](eid: EntityId; pos: float2; is_alive: bool)
    if is_alive
      invoke(blk, [[AlivePlayer eid=eid, pos=pos]])

[es]
def bar(evt: EventUpdate)
  foreach_alive_player() <| $(player: AlivePlayer)
    print("Player: {player.eid}")

[es (after=apply_controls)]
def apply_gravity(evt: EventUpdate; var vel: float2&; gravity: Gravity; is_alive: bool)
  if is_alive
    vel.y += gravity.mass * 9.8f * evt.dt

// [es (have=user_input)]
// def foo1(evt: EventUpdate; eid: EntityId; pos: float2; gravity: Gravity)
//   print("foo1[{eid}]: {pos}; {gravity.mass}")

// [es (after=foo1)]
// def foo2(evt: EventUpdate)
//   print("foo2: {evt.dt}; {evt.total}")

// [es (before=foo1)]
// def foo3(evt: EventUpdate)
//   query() <| $[es (have=user_input)](eid: EntityId; pos: float2; gravity: Gravity)
//     print("foo3[{eid}]: {pos}; {gravity.mass}")

[init]
def main()
  print("main.das has been loaded")