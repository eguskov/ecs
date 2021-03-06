#include "level_1.as"

[query { "$have": [ "wall" ] }]
class Wall
{
  const EntityId@ eid;
}

[query { "$have": [ "zone" ] }]
class Zone
{
  const EntityId@ eid;
}

[query { "$have": [ "enemy" ] }]
class Enemy
{
  const EntityId@ eid;
}

[query { "$have": [ "trigger" ] }]
class Trigger
{
  const EntityId@ eid;
}

[query { "$have": [ "action" ] }]
class Action
{
  const EntityId@ eid;
}

void create_level()
{
  create_entities_from_file("data/level_1.json");

  // int w = 0;
  // Array@ pattern = get_level(w);

  // create_entity("enable-lift-action", Map = {
  //   {"key", "enable-lift-1"},
  //   {"lift_key", "go-up-lift"}
  // });

  // create_entity("enable-lift-action", Map = {
  //   {"key", "enable-lift-2"},
  //   {"lift_key", "go-down-lift"}
  // });

  // for (int i = 0; i < pattern.size(); ++i)
  // {
  //   int x = i % w;
  //   int y = i / w;
  //   int p = pattern.getInt(i);

  //   float wx = float(x) * 32.f;
  //   float wy = float(y) * 32.f;

  //   if (p == 1)
  //   {
  //     create_entity("block-big", Map = {
  //       {"pos", Array = {wx, wy}}
  //     });
  //   }
  //   else if (p == 2)
  //   {
  //     create_entity("player_spawn_zone", Map = {
  //       {"pos", Array = {wx, wy}}
  //     });
  //   }
  //   else if (p == 3)
  //   {
  //     create_entity("block-big-moving", Map = {
  //       {"pos", Array = {wx, wy}},
  //       {"vel", Array = {0.f, -1.f}},
  //       {"is_active", false},
  //       {"key", "go-up-lift"},
  //       {"auto_move", Map = { {"duration", 2.f}, {"length", 2.f * 32.f}, {"jump", false} } }
  //     });
  //   }
  //   else if (p == 4)
  //   {
  //     create_entity("block-big-moving", Map = {
  //       {"pos", Array = {wx, wy}},
  //       {"vel", Array = {0.f, 1.f}},
  //       {"is_active", false},
  //       {"key", "go-down-lift"},
  //       {"auto_move", Map = { {"duration", 2.f}, {"length", 7.f * 32.f}, {"jump", false} } }
  //     });
  //   }
  //   else if (p == 5)
  //   {
  //     create_entity("switch-trigger", Map = {
  //       {"pos", Array = {wx, wy}},
  //       {"key", "enable-lift-trigger-1"},
  //       {"action_key", "enable-lift-1"}
  //     });
  //   }
  //   else if (p == 6)
  //   {
  //     create_entity("switch-trigger", Map = {
  //       {"pos", Array = {wx, wy}},
  //       {"key", "enable-lift-trigger-2"},
  //       {"action_key", "enable-lift-2"}
  //     });
  //   }
  //   else if (p == 10)
  //   {
  //     create_entity("opossum", Map = {
  //       {"pos", Array = {wx, wy + 4.f}},
  //       {"vel", Array = {-20.f, 0.f}}
  //     });
  //   }
  // }
}

[on_load]
void load()
{
  print("level_generator.as: load");
  create_level();
}

[on_reload]
void reload()
{
  print("level_generator.as: reload");

  int count = 0;
  for (auto it = Query<Wall>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  for (auto it = Query<Zone>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  for (auto it = Query<Enemy>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  for (auto it = Query<Action>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  for (auto it = Query<Trigger>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  print("level_generator.as: deleted: " + count);

  create_level();
}