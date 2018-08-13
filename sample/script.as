void script_system(const UpdateStage&in stage, const vec2&in pos, const vec2&in vel)
{
  vec2 res;
  res.x = pos.x + vel.x * stage.dt;
  res.y = pos.y + vel.y * stage.dt;
}

ref@ main()
{
  print("main");

  array<ref@> systems;
  systems.insertLast(@script_system);
  return systems;
}