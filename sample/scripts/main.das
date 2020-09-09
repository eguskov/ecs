require ecs
require sample

[es (after=apply_controls)]
def apply_gravity(evt: EventUpdate; var vel: float2&; gravity: Gravity; is_alive: bool)
  if (is_alive)
    vel.y += gravity.mass * 9.8f * evt.dt;

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