#include "level_1.as"

[query { "$have": [ "wall" ] }]
class AllBlocksQuery
{
  const EntityId@ eid;
}

void create_level()
{
  int w = 0;
  Array@ pattern = get_level(w);

  for (int i = 0; i < pattern.size(); ++i)
  {
    int x = i % w;
    int y = i / w;
    int p = pattern.getInt(i);

    float wx = float(x) * 32.f;
    float wy = float(y) * 32.f;

    if (p == 1)
    {
      create_entity("block-big", Map = {
        {"pos", Array = {wx, wy}}
      });
    }
    else if (p == 2)
    {
      create_entity("player_spawn_zone", Map = {
        {"pos", Array = {wx, wy}}
      });
    }
    else if (p == 3)
    {
      create_entity("block-big-moving", Map = {
        {"pos", Array = {wx, wy}},
        {"vel", Array = {0.f, -1.f}},
        {"auto_move", Map = { {"duration", 2.f}, {"length", 2.f * 32.f}, {"jump", false} } }
      });
    }
    else if (p == 4)
    {
      create_entity("block-big-moving", Map = {
        {"pos", Array = {wx, wy}},
        {"vel", Array = {0.f, 1.f}},
        {"auto_move", Map = { {"duration", 2.f}, {"length", 7.f * 32.f}, {"jump", false} } }
      });
    }
  }
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
  for (auto it = Query<AllBlocksQuery>().perform(); it.hasNext(); ++it)
  {
    auto @block = it.get();
    ++count;
    delete_entity(block.eid);
  }

  print("level_generator.as: " + count);

  create_level();
}