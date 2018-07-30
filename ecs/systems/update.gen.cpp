#include "update.cpp"

template <> struct Desc<decltype(update_position)> { constexpr static char* name = "update_position"; };
static RegSysSpec<decltype(update_position)> _reg_sys_update_position("update_position", &update_position, { "stage", "vel", "pos" });

template <> struct Desc<decltype(update_vels)> { constexpr static char* name = "update_vels"; };
static RegSysSpec<decltype(update_vels)> _reg_sys_update_vels("update_vels", &update_vels, { "stage", "vels" });

template <> struct Desc<decltype(update_velocity)> { constexpr static char* name = "update_velocity"; };
static RegSysSpec<decltype(update_velocity)> _reg_sys_update_velocity("update_velocity", &update_velocity, { "stage", "damping", "pos", "pos_copy", "vel" });

template <> struct Desc<decltype(update_collisions)> { constexpr static char* name = "update_collisions"; };
static RegSysSpec<decltype(update_collisions)> _reg_sys_update_collisions("update_collisions", &update_collisions, { "stage", "eid", "mass", "gravity", "pos", "vel" });

template <> struct Desc<decltype(spawner)> { constexpr static char* name = "spawner"; };
static RegSysSpec<decltype(spawner)> _reg_sys_spawner("spawner", &spawner, { "stage", "eid", "timer" });

template <> struct Desc<decltype(render)> { constexpr static char* name = "render"; };
static RegSysSpec<decltype(render)> _reg_sys_render("render", &render, { "stage", "eid", "color", "pos" });

template <> struct Desc<decltype(spawn_handler)> { constexpr static char* name = "spawn_handler"; };
static RegSysSpec<decltype(spawn_handler)> _reg_sys_spawn_handler("spawn_handler", &spawn_handler, { "ev", "eid", "vel", "pos" });

