void script_system(const UpdateStage& stage, const vec2& pos, vec2& vel)
{
  //vec2 res = pos + vel * stage.dt;
  //vel.x += 0.5;
  // pos + vel * stage.dt;
  //vec2 res = pos + vel * stage.dt;
  //float a = dot(pos, vel);

  // print("script_system: pos: ("+pos.x+", "+pos.y+")");
  // print("script_system: vel: ("+vel.x+", "+vel.y+")");
}

void on_enenmy_kill_handler(const EventOnKillEnemy& ev, const vec2& pos)
{
  print("on_enenmy_kill_handler");
  print("on_enenmy_kill_handler: pos: ("+pos.x+", "+pos.y+")");
}

void on_enenmy_hit_wall_handler(const EventOnWallHit& ev, const vec2& pos, vec2& vel, real& dir)
{
  if (dot(ev.normal, vel) >= 0.f)
    return;
    
  print("on_enenmy_hit_wall_handler");
  print("on_enenmy_hit_wall_handler: dir: ("+dir.v+")");

  if (ev.normal.x < 0.f || ev.normal.x > 0.f)
  {
    vel.x = ev.normal.x * abs(vel.x);
    dir = -dir;
  }
}

ref@ main()
{
  print("main");

  array<ref@> systems;
  systems.insertLast(@script_system);
  systems.insertLast(@on_enenmy_kill_handler);
  systems.insertLast(@on_enenmy_hit_wall_handler);
  return systems;
}