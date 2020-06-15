[on_load]
void load()
{
  print("script.as: load");

  create_entity("script", Map = {
    {"script", Map = {{"name", "level_generator"}, {"path", "scripts/level_generator.as"}}}
  });
}