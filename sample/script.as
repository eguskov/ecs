[query { "$have": "enemy", "$is-true": "is_alive" }]
class AliveEnemy
{
  const EntityId@ eid;
  const vec2@ pos;
  const vec2@ vel;
}

[query { "$is-true": "is_alive", "$have": "user_input" }]
class AlivePlayerQuery
{
}

[query { "$have": "player_spawn_zone" }]
class PlayerSpawnZonesQuery
{
  const vec2@ pos;
}

[query { "$is-true": "is_alive", "$have": "enemy" }]
class AliveEnemiesCountQuery
{
}

[query { "$have": "user_input" }]
class Player
{
  const EntityId@ eid;
  const vec2@ pos;
  const vec2@ vel;
}

[system { "$link": [ "player.enemy_eid", "enemy.eid" ] }]
void process_player_test(const UpdateStage@ stage, const Player@ player, const AliveEnemy@ enemy)
{
  // print("process_player_test: player: "+player.eid.handle);
  // print("process_player_test: enemy: "+enemy.eid.handle);
}

// [system { "$is-false": "use_car_trigger_active" }]
// void check_trigger(const UpdateStage@ stage, const Trigger@ use_car_trigger, boolean@ use_car_trigger_active)
// {
//   use_car_trigger_active = true;
// }

// [system { "$is-true": "caruse_car_trigger_active" }]
// void select_cars_with_active_trigger(const UpdateStage@ stage, Player@ player, Car@ car)
// {
//   for (auto it = Query<AlivePlayerQuery>().perform(); it.hasNext(); ++it)
//     it.get().car_eid = eid;
// }

// [system { "$eq": [ "player.car_eid", "car.eid" ], "$is-true": "player.is_alive" }]
// void join_system(const UpdateStage@ stage, Player@ player, Car@ car)
// {
// }

[system { "$have": "spawner" }]
void update_spawner(const UpdateStage@ stage, TimerComponent@ spawn_timer)
{
  // TODO: Implement more convenient way to do this
  bool hasPlayer = false;
  for (auto it = Query<AlivePlayerQuery>().perform(); it.hasNext(); ++it)
    hasPlayer = true;

  if (!hasPlayer)
    return;

  for (auto it = Query<AliveEnemiesCountQuery>().perform(); it.hasNext(); ++it)
    return;

  spawn_timer.time -= stage.dt;
  if (spawn_timer.time < 0.f)
  {
    spawn_timer.time = spawn_timer.period;

    // print("Spawn!");

    // create_entity("opossum", Map = {
    //   {"pos", Array = { 0.f, 4.f }},
    //   {"vel", Array = { -20.f, 0.f }}
    // });

    // create_entity("eagle", Map = {
    //   {"pos", Array = { 128.f, -80.f }},
    //   {"vel", Array = { 0.f, -1.f }}
    // });

    // create_entity("eagle", Map = {
    //   {"pos", Array = { -64.f, -96.f }},
    //   {"vel", Array = { -1.f, 0.f }}
    // });

    // create_entity("frog", Map = {
    //   {"pos", Array = { 0.f, 6.f }},
    //   {"vel", Array = { 0.f, 0.f }}
    // });
  }
}

[system { "$have": "player_spawner" }]
void update_player_spawner(const UpdateStage@ stage)
{
  for (auto it = Query<AlivePlayerQuery>().perform(); it.hasNext(); ++it)
    return;

  const vec2@ spawnPos;
  for (auto it = Query<PlayerSpawnZonesQuery>().perform(); it.hasNext(); ++it)
    @spawnPos = it.get().pos;

  if (spawnPos is null)
    return;

  print("Spawn player");

  create_entity("fox", Map = {
    {"pos", Array = {spawnPos.x, spawnPos.y}},
    {"vel", Array = {0.f, 0.f}}
  });
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

[on_load]
void load()
{
  print("script.as: load");

  create_entity("script", Map = {
    {"script", Map = {{"name", "level_generator"}, {"path", "level_generator.as"}}}
  });
}

[on_reload]
void reload()
{
  print("script.as: reload");
}