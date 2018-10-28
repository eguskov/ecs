[query { "$is-true": "is_alive" }]
class AliveEnemy
{
  const AutoMove@ auto_move;
  const vec2@ pos;
  vec2@ vel;
}

[query { "$is-true": "is_alive", "$have": "enemy" }]
class AliveEnemiesCountQuery
{
}

[system]
void update_spawner(const UpdateStage@ stage, TimerComponent@ spawn_timer)
{
  int count = 0;
  for (auto it = Query<AliveEnemiesCountQuery>().perform(); it.hasNext(); ++it)
  {
    ++count;
  }

  print("count: "+count);

  if (count == 0)
  {
    spawn_timer.time -= stage.dt;
    if (spawn_timer.time < 0.f)
    {
      spawn_timer.time = spawn_timer.period;
    }
  }
}

[system { "$have": "enemy" }]
void on_enenmy_kill_handler(const EventOnKillEnemy@ ev, const vec2@ pos)
{
  print("on_enenmy_kill_handler: pos: ("+pos.x+", "+pos.y+")");

  create_entity("death_fx", Map = {
    {"pos", Array = { ev.pos.x, ev.pos.y }}
  });
}

[system { "$have": "enemy" }]
void on_enenmy_hit_wall_handler(const EventOnWallHit@ ev, vec2@ pos, vec2@ vel, real@ dir)
{
  if (dot(ev.normal, vel) >= 0.f)
    return;

  if (ev.normal.x < 0.f || ev.normal.x > 0.f)
  {
    vel.x = ev.normal.x * abs(vel.x);
    dir = -float(dir);
  }
}

[system { "$have": "enemy" }]
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

[system { "$is-true": "is_alive" }]
void update_auto_jump(const UpdateStage@ stage, const boolean@ is_alive, Jump@ jump, AutoMove@ auto_move, vec2@ vel, real@ dir)
{
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

[system { "$is-true": "is_alive" }]
void update_auto_move(const UpdateStage@ stage, const boolean@ is_alive, AutoMove@ auto_move, vec2@ vel, real@ dir)
{
  for (auto it = Query<AliveEnemy>().perform(); it.hasNext(); ++it)
  {
    auto @enemy = it.get();
    // enemy.vel.x += 0.5;
    // print("query: enemy.pos: (" + enemy.pos.x + ", " + enemy.pos.y +")");
    // print("query: enemy.vel: (" + enemy.vel.x + ", " + enemy.vel.y +")");
  }

  if (!auto_move.jump)
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

void main()
{
  print("main");
}