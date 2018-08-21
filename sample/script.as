void on_enenmy_kill_handler(const EventOnKillEnemy@ ev, const vec2@ pos)
{
  print("on_enenmy_kill_handler: pos: ("+pos.x+", "+pos.y+")");
}

void on_enenmy_hit_wall_handler(const EventOnWallHit@ ev, vec2@ pos, vec2@ vel, real@ dir)
{
  if (dot(ev.normal, vel) >= 0.f)
    return;

  if (ev.normal.x < 0.f || ev.normal.x > 0.f)
  {
    vel.x = ev.normal.x * abs(vel.x);
    dir = -dir;
  }
}

void on_enenmy_hit_ground_handler(const EventOnWallHit@ ev, vec2@ pos, vec2@ vel, real@ dir, boolean@ is_on_ground)
{
  if (dot(ev.normal, vel) >= 0.f)
    return;

  if (ev.normal.y < 0.f)
  {
    is_on_ground = true;

    const float proj = dot(vel, ev.normal);
    if (proj < 0.f)
      vel = vel + abs(proj) * ev.normal;
    pos = pos + abs(ev.d) * ev.normal;
    vel.x = 0.f;
  }
}

void update_auto_jump(const UpdateStage@ stage, const boolean@ is_alive, Jump@ jump, AutoMove@ auto_move, vec2@ vel, real@ dir)
{
  if (!bool(is_alive))
    return;

  if (auto_move.jump)
  {
    auto_move.time -= stage.dt;
    if (auto_move.time < 0.f)
    {
      auto_move.time = auto_move.duration;

      jump.active = true;
      jump.startTime = stage.total;

      vel.x = 40.f * -dir.v;
      vel.y = -40.f;
    }
  }
}

void update_auto_move(const UpdateStage@ stage, const boolean@ is_alive, AutoMove@ auto_move, vec2@ vel, real@ dir)
{
  if (!bool(is_alive))
    return;

  if (auto_move.jump)
  {

  }
  else
  {
    if (length(vel) > 0.f)
      vel = (auto_move.length / auto_move.duration) * normalize(vel);

    auto_move.time -= stage.dt;
    if (auto_move.time < 0.f)
    {
      auto_move.time = auto_move.duration;
      vel = -vel;
    }
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
  systems.insertLast(@update_auto_jump);
  systems.insertLast(@update_auto_move);
  systems.insertLast(@on_enenmy_kill_handler);
  systems.insertLast(@on_enenmy_hit_wall_handler);
  systems.insertLast(@on_enenmy_hit_ground_handler);
  return systems;
}