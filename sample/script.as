void on_enenmy_kill_handler(const EventOnKillEnemy@ ev, const vec2@ pos)
{
  print("on_enenmy_kill_handler");
  print("on_enenmy_kill_handler: pos: ("+pos.x+", "+pos.y+")");
}

void on_enenmy_hit_wall_handler(const EventOnWallHit@ ev, const vec2@ pos, vec2@ vel, real@ dir)
{
  if (dot(ev.normal, vel) >= 0.f)
    return;

  print("on_enenmy_hit_wall_handler: dir: ("+dir.v+")");

  if (ev.normal.x < 0.f || ev.normal.x > 0.f)
  {
    print("on_enenmy_hit_wall_handler: change dir: normal.x: " + ev.normal.x);
    const float wasVel = vel.x;
    vel.x = ev.normal.x * abs(vel.x);
    dir = -dir;
    print("on_enenmy_hit_wall_handler: change dir: vel.x: " + wasVel + " -> " + vel.x);
  }
}

void update_auto_move(const UpdateStage@ stage, const boolean@ is_alive, AutoMove@ auto_move, vec2@ vel, real@ dir)
{
  if (!bool(is_alive))
    return;

  if (length(vel) > 0.f)
  {
    vel = (auto_move.length / auto_move.duration) * normalize(vel);
  }

  auto_move.time -= stage.dt;
  if (auto_move.time < 0.f)
  {
    auto_move.time = auto_move.duration;
    vel = -vel;
  }

  if (vel.x < 0.f)
    dir = 1.f;
  else if (vel.x > 0.f)
    dir = -1.f;
}

ref@ main()
{
  print("main");

  array<ref@> systems;
  systems.insertLast(@update_auto_move);
  systems.insertLast(@on_enenmy_kill_handler);
  systems.insertLast(@on_enenmy_hit_wall_handler);
  return systems;
}