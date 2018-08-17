void script_system(const UpdateStage& stage, const vec2& pos, vec2& vel)
{
  vel.x += 0.5;
  // pos + vel;
  // vec2 res = pos + vel;// * stage.dt;

  // print("script_system: pos: ("+pos.x+", "+pos.y+")");
  // print("script_system: vel: ("+vel.x+", "+vel.y+")");
}

void on_enenmy_kill_handler(const EventOnKillEnemy& ev, const vec2& pos)
{
  print("on_enenmy_kill_handler");
  print("on_enenmy_kill_handler: pos: ("+pos.x+", "+pos.y+")");
}

ref@ main()
{
  print("main");

  array<ref@> systems;
  systems.insertLast(@script_system);
  systems.insertLast(@on_enenmy_kill_handler);
  return systems;
}