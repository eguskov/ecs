require ecs
require anim
require render
require phys
require sample
require raylib

[ecsTemplate (name="physics_world")]
def physics_world(var cm: ComponentsMap)
  add(cm, "phys_world", [[PhysicsWorld]])

[ecsTemplate (name="death_fx")]
def death_fx(var cm: ComponentsMap)
  add(cm, "death_fx", [[Tag]])
  add(cm, "pos", float2())
  add(cm, "dir", 1.f)
  add(cm, "is_alive", true)
  add(cm, "texture_path", "../assets/atlas/atlas.png")
  add(cm, "texture_id", [[Texture2D]])
  add(cm, "frame", float4())

  add(cm, "anim_state", [[AnimState node:=*eastl_string("death")]])
  add(cm, "anim_graph") <|
    var ag = new AnimGraph
    let frames = [[auto
      float4(248, 102, 40, 41);
      float4(122, 102, 40, 41);
      float4(206, 102, 40, 41);
      float4(164, 102, 40, 41);
      float4( 80, 102, 40, 41);
      float4( 38, 102, 40, 41)
    ]]
    add_node(ag, "death", 10.f, false, to_array(frames))
    return ag

[ecsTemplate (name="enemy_spawner")]
def enemy_spawner(var cm: ComponentsMap)
  add(cm, "spawner", [[Tag]])

[ecsTemplate (name="player_spawner")]
def player_spawner(var cm: ComponentsMap)
  add(cm, "player_spawner", [[Tag]])

[ecsTemplate (name="player_spawn_zone")]
def player_spawn_zone(var cm: ComponentsMap)
  add(cm, "zone", [[Tag]])
  add(cm, "player_spawn_zone", [[Tag]])
  add(cm, "pos", float2())

def enemy(var cm: ComponentsMap)
  add(cm, "enemy", [[Tag]])
  add(cm, "grid_cell", -1)
  add(cm, "pos", float2())
  add(cm, "vel", float2())
  add(cm, "dir", 1.f)
  add(cm, "is_alive", true)

def add_collision_shape(var cm; size, center)
  add(cm, "collision_shape") <|
    var shape = new CollisionShape
    add_shape(shape, "box", size, center, 0.f/* angle */, 1.f /* density */, 0.5f /* friction */)
    return shape

def add_collision_shape_with_sensor(var cm; size, center, sensor_size, sensor_center)
  add(cm, "collision_shape") <|
    var shape = new CollisionShape
    add_shape (shape, "box", size, center, 0.f/* angle */, 1.f /* density */, 0.5f /* friction */)
    add_sensor(shape, "box", sensor_size, sensor_center)
    return shape

def add_collision_sensor(var cm; size, center)
  add(cm, "collision_shape") <|
    var shape = new CollisionShape
    add_sensor(shape, "box", size, center)
    return shape

[ecsTemplate (name="opossum")]
def opossum(var cm: ComponentsMap)
  enemy(cm)

  add(cm, "texture_path", "../assets/atlas/atlas.png")
  add(cm, "texture_id", [[Texture2D]])
  add(cm, "frame", float4())

  add(cm, "auto_move_jump", false)
  add(cm, "auto_move_duration", 2.f)
  add(cm, "auto_move_length", 64.f)
  add(cm, "auto_move_time", 0.f)
  add_collision_shape(cm, float2(36.f, 20.f), float2(18.f, 14.f))

  // TODO: add string component phys_body_type and use in event on create
  // instead of create_phys_body("kinematic")
  add(cm, "phys_body", create_phys_body("kinematic"))
  add(cm, "is_on_ground", true)

  add(cm, "anim_state", [[AnimState node:=*eastl_string("run")]])
  add(cm, "anim_graph") <|
    var ag = new AnimGraph
    let frames = [[auto
      float4(368,  68, 36, 28);
      float4(292,  68, 36, 28);
      float4(330,  68, 36, 28);
      float4(406,  68, 36, 28);
      float4(  0, 102, 36, 28);
      float4(444,  68, 36, 28)
    ]]
    add_node(ag, "run", 10.f, true, to_array(frames))
    return ag

[ecsTemplate (name="eagle")]
def eagle(var cm: ComponentsMap)
  enemy(cm)

  add(cm, "texture_path", "../assets/atlas/atlas.png")
  add(cm, "texture_id", [[Texture2D]])
  add(cm, "frame", float4())

  add(cm, "auto_move_jump", false)
  add(cm, "auto_move_duration", 2.f)
  add(cm, "auto_move_length", 64.f)
  add(cm, "auto_move_time", 0.f)
  add_collision_shape(cm, float2(36.f, 36.f), float2(18.f, 18.f))

  // TODO: add string component phys_body_type and use in event on create
  // instead of create_phys_body("kinematic")
  add(cm, "phys_body", create_phys_body("kinematic"))
  add(cm, "is_on_ground", true)

  add(cm, "anim_state", [[AnimState node:=*eastl_string("run")]])
  add(cm, "anim_graph") <|
    var ag = new AnimGraph
    let frames = [[auto
      float4(332, 102, 40, 41);
      float4(416, 102, 40, 41);
      float4(374, 102, 40, 41);
      float4(290, 102, 40, 41)
    ]]
    add_node(ag, "run", 10.f, true, to_array(frames))
    return ag

[ecsTemplate (name="frog")]
def frog(var cm: ComponentsMap)
  enemy(cm)

  add(cm, "wall_collidable", [[Tag]])
  add(cm, "dir", -1.f)

  add(cm, "texture_path", "../assets/atlas/atlas.png")
  add(cm, "texture_id", [[Texture2D]])
  add(cm, "frame", float4())

  add(cm, "auto_move_jump", true)
  add(cm, "auto_move_duration", 2.f)
  add(cm, "auto_move_length", 64.f)
  add(cm, "auto_move_time", 0.f)
  add_collision_shape(cm, float2(36.f, 28.f), float2(18.f, 14.f))

  // TODO: add string component phys_body_type and use in event on create
  // instead of create_phys_body("kinematic")
  add(cm, "phys_body", create_phys_body("kinematic"))
  add(cm, "is_on_ground", true)
  add(cm, "jump_active", false)
  add(cm, "jump_startTime", 0.lf)
  add(cm, "jump_height", 15.f)
  add(cm, "jump_duration", 0.1f)
  add(cm, "mass", 25.f)

  add(cm, "anim_state", [[AnimState node:=*eastl_string("idle")]])
  add(cm, "anim_graph") <|
    var ag = new AnimGraph
    let idleFrames = [[auto
      float4(181, 68, 35, 32);
      float4(255, 68, 35, 32);
      float4(144, 68, 35, 32);
      float4(107, 68, 35, 32)
    ]]
    add_node(ag, "idle",  8.f, true, to_array(idleFrames))
    add_node(ag, "jump", 10.f, true, to_array([[float4[1] float4(218, 68, 35, 32)]]))
    add_node(ag, "fall", 10.f, true, to_array([[float4[1] float4( 70, 68, 35, 32)]]))
    return ag

[ecsTemplate (name="fox")]
def fox(var cm: ComponentsMap)
  add(cm, "grid_cell", -1)
  add(cm, "user_input", [[UserInput]])
  add(cm, "mass", 25.f)
  add(cm, "jump_active", false)
  add(cm, "jump_startTime", 0.lf)
  add(cm, "jump_height", 25.f)
  add(cm, "jump_duration", 0.1f)
  add(cm, "pos", float2())
  add(cm, "vel", float2())
  add(cm, "dir", 1.f)
  add(cm, "is_alive", true)
  add(cm, "is_on_ground", true)
  add(cm, "frame", float4())
  add(cm, "texture_path", "../assets/atlas/atlas.png")
  add(cm, "texture_id", [[Texture2D]])

  add_collision_shape(cm, float2(20.f, 32.f), float2(16.f, 16.f))

  add(cm, "phys_body", create_phys_body("dynamic"))

  add(cm, "anim_state", [[AnimState node:=*eastl_string("idle")]])
  add(cm, "anim_graph") <|
    var ag = new AnimGraph
    let idleFrames = [[auto
      float4(105, 34, 33, 32);
      float4(350, 34, 33, 32);
      float4(452,  0, 33, 32);
      float4(315, 34, 33, 32)
    ]]
    add_node(ag, "idle", 10.f, true, to_array(idleFrames))
    let runFrames = [[auto
      float4(385, 34, 33, 32);
      float4( 35, 68, 33, 32);
      float4(  0, 68, 33, 32);
      float4(420, 34, 33, 32);
      float4(455, 34, 33, 32);
      float4(417,  0, 33, 32)
    ]]
    add_node(ag, "run", 10.f, true, to_array(runFrames))
    add_node(ag, "jump", 10.f, true, to_array([[float4[1] float4(35, 34, 33, 32)]]))
    add_node(ag, "fall", 10.f, true, to_array([[float4[1] float4( 0, 34, 33, 32)]]))
    return ag

[ecsTemplate (name="base-block")]
def base_block(var cm: ComponentsMap)
  add(cm, "wall", [[Tag]])
  add(cm, "grid_cell", -1)
  add(cm, "pos", float2())
  add(cm, "dir", 1.f)
  add(cm, "is_alive", true)
  add(cm, "frame", float4())

  add_collision_shape(cm, float2(32.f, 32.f), float2(16.f, 16.f))

  add(cm, "phys_body", create_phys_body("static"))

[ecsTemplate (name="base-textured-block")]
def base_textured_block(var cm: ComponentsMap)
  base_block(cm)
  add(cm, "texture_path", "../assets/atlas/atlas-props.png")
  add(cm, "texture_id", [[Texture2D]])

[ecsTemplate (name="block-big")]
def block_big(var cm: ComponentsMap)
  base_textured_block(cm)
  add(cm, "frame", float4(2, 93, 32, 32))

[ecsTemplate (name="block-big-moving")]
def block_big_moving(var cm: ComponentsMap)
  block_big(cm)
  add(cm, "lift", [[Tag]])
  add(cm, "vel", float2(0.f, 1.f))
  add(cm, "is_active", true)
  add(cm, "key", 0)
  add(cm, "auto_move_jump", false)
  add(cm, "auto_move_duration", 2.f)
  add(cm, "auto_move_length", 64.f)
  add(cm, "auto_move_time", 0.f)
  add_collision_shape_with_sensor(cm, float2(32.f, 32.f), float2(16.f, 16.f), float2(24.f, 16.f), float2(16.f, -5.f))
  add(cm, "phys_body", create_phys_body("kinematic"))

[ecsTemplate (name="block-big-cage")]
def block_big_cage(var cm: ComponentsMap)
  block_big(cm)
  add(cm, "cage", [[Tag]])

[ecsTemplate (name="block")]
def common_block(var cm: ComponentsMap)
  base_textured_block(cm)
  add(cm, "frame", float4(84, 19, 16, 16))
  add_collision_shape(cm, float2(16.f, 16.f), float2(8.f, 8.f))

[ecsTemplate (name="ground-block")]
def ground_block(var cm: ComponentsMap)
  base_textured_block(cm)
  add(cm, "texture_path", "../assets/environment/tileset.png")
  add(cm, "texture_id", [[Texture2D]])
  add(cm, "frame", float4(16, 16, 16, 16))
  add_collision_shape(cm, float2(16.f, 16.f), float2(8.f, 8.f))

[ecsTemplate (name="trigger")]
def trigger(var cm: ComponentsMap)
  add(cm, "trigger", [[Tag]])
  add(cm, "key", 0)
  add(cm, "is_binded", false)
  add(cm, "is_active", false)
  add(cm, "action_eid", [[EntityId]])
  return

[ecsTemplate (name="switch-trigger")]
def switch_trigger(var cm: ComponentsMap)
  trigger(cm)
  add(cm, "switch_trigger", [[Tag]])
  add(cm, "action_key", 0)
  add(cm, "pos", float2())
  add_collision_sensor(cm, float2(32.f, 32.f), float2(16.f, 16.f))
  add(cm, "phys_body", create_phys_body("static"))

[ecsTemplate (name="zone-trigger")]
def zone_trigger(var cm: ComponentsMap)
  trigger(cm)
  add(cm, "zone_trigger", [[Tag]])
  add(cm, "action_key", 0)
  add(cm, "pos", float2())
  add(cm, "collision_rect", float4(0.f, 0.f, 32.f, 20.f))

[ecsTemplate (name="action")]
def action(var cm: ComponentsMap)
  add(cm, "action", [[Tag]])
  add(cm, "key", 0)
  add(cm, "is_active", false)

[ecsTemplate (name="enable-lift-action")]
def enable_lift_action(var cm: ComponentsMap)
  action(cm)
  add(cm, "enable_lift_action", [[Tag]])
  add(cm, "lift_key", 0)

[ecsTemplate (name="kill-player-action")]
def kill_player_action(var cm: ComponentsMap)
  action(cm)
  add(cm, "kill_player_action", [[Tag]])

[ecsTemplate (name="open-cage-action")]
def open_cage_action(var cm: ComponentsMap)
  action(cm)
  add(cm, "open_cage_action", [[Tag]])