[query { "$is-true": "is_alive" }]
class AliveEnemy
{
  const AutoMove@ auto_move;
  const vec2@ pos;
  vec2@ vel;
}

// TODO: Make real usage of query
// for (auto it = Query<AliveEnemy>().perform(); it.hasNext(); ++it)
// {
//   auto @enemy = it.get();
//   enemy.vel.x += 0.5;
//   print("query: enemy.pos: (" + enemy.pos.x + ", " + enemy.pos.y +")");
//   print("query: enemy.vel: (" + enemy.vel.x + ", " + enemy.vel.y +")");
// }

[query { "$is-true": "is_alive", "$have": "enemy" }]
class AliveEnemiesCountQuery
{
}

[system { "$have": "spawner" }]
void update_spawner(const UpdateStage@ stage, TimerComponent@ spawn_timer)
{
  for (auto it = Query<AliveEnemiesCountQuery>().perform(); it.hasNext(); ++it)
    return;

  spawn_timer.time -= stage.dt;
  if (spawn_timer.time < 0.f)
  {
    spawn_timer.time = spawn_timer.period;

    print("Spawn!");

    create_entity("opossum", Map = {
      {"pos", Array = { 0.f, 4.f }},
      {"vel", Array = { -20.f, 0.f }}
    });

    create_entity("eagle", Map = {
      {"pos", Array = { 128.f, -80.f }},
      {"vel", Array = { 0.f, -1.f }}
    });

    create_entity("eagle", Map = {
      {"pos", Array = { -64.f, -96.f }},
      {"vel", Array = { -1.f, 0.f }}
    });

    create_entity("frog", Map = {
      {"pos", Array = { 0.f, 6.f }},
      {"vel", Array = { 0.f, 0.f }}
    });
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
void on_enenmy_hit_wall_handler(const EventOnWallHit@ ev, vec2@ vel, real@ dir)
{
  if (dot(ev.normal, ev.vel) >= 0.f)
    return;

  if (ev.normal.x < 0.f || ev.normal.x > 0.f)
  {
    vel.x = ev.normal.x * abs(ev.vel.x);
    dir = -float(dir);
  }
}

[system { "$have": "enemy" }]
void on_enenmy_hit_ground_handler(const EventOnWallHit@ ev, vec2@ vel, boolean@ is_on_ground)
{
  if (dot(ev.normal, ev.vel) >= 0.f)
    return;

  if (ev.normal.y < 0.f)
  {
    is_on_ground = true;
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