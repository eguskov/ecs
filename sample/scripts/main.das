require ecs
require sample
require math

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

// TODO: Macro?
// struct AlivePlayer
//   eid: EntityId
//   pos: float2

// def foreach_alive_player(blk: block<(AlivePlayer):void>)
//   query() <| $[es (have=user_input)](eid: EntityId; pos: float2; is_alive: bool)
//     if is_alive
//       invoke(blk, [[AlivePlayer eid=eid, pos=pos]])

// [es]
// def bar(evt: EventUpdate)
//   foreach_alive_player() <| $(player: AlivePlayer)
//     print("Player: {player.eid}")

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

[es (after=read_controls)]
def apply_controls(evt: EventUpdate;
                   user_input: UserInput;
                   is_on_ground: bool;
                   var jump_active: bool&;
                   var jump_startTime: double&;
                   var vel: float2&;
                   var dir: float&)
  if user_input.left
    vel.x = -50.f
    dir = -1.f
  elif user_input.right
    vel.x = 50.f
    dir = 1.f
  else
    vel.x = 0.f

  if user_input.jump && !jump_active && is_on_ground
    jump_active = true
    jump_startTime = evt.total

[es (after=apply_controls)]
def apply_gravity(evt: EventUpdate;
                  is_alive: bool;
                  mass: float;
                  var vel: float2&)
  if is_alive
    vel.y += mass * 9.8f * evt.dt

[es (after=apply_gravity)]
def apply_jump(evt:EventUpdate;
               jump_startTime: double;
               jump_duration: float;
               jump_height: float;
               var jump_active: bool&;
               var is_on_ground: bool&;
               var vel: float2&)
  if !jump_active
    return

  is_on_ground = false

  let k = clamp((evt.total - jump_startTime) / double(jump_duration), 0.lf, 1.lf)
  let v = jump_height / jump_duration
  vel.y = -v;

  if k >= 1.lf
    vel.y *= 0.5f
    jump_active = false

[init]
def main()
  print("main.das has been loaded")