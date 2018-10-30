// [query { $have": "wall" }]
// class AllWallsQuery
// {
//   EntityId eid;
// }

// [system]
// void generate_level(const CmdGenerateLevel@ ev, const LevelGenerator@ level_generator)
// {
//   for (auto it = Query<AllWallsQuery>().perform(); it.hasNext(); ++it)
//   {
//     delete_entity(it.get().eid);
//   }

//   for (int x = 0; x < 10; ++x)
//   {
//     create_entity("block-big", Map = {
//       {"pos", Array = { float(x) * 32.f, 0.f }}
//     });
//   }
// }

[on_load]
void load()
{
  print("level_generator.as: load");

//   create_entity("script", Map = {
//     {"script", Map = {{"name", "level_generator"}, {"path", "level_generator.as"}}}
//   });

//   create_entity("fox", Map = {
//     {"pos", Array = {0.f, -128.f}},
//     {"vel", Array = {0.f, 0.f}}
//   });
}

[on_reload]
void reload()
{
  print("level_generator.as: reload");
}