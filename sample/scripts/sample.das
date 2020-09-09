// options log = true, log_nodes = true

require ecs
require anim
require render
require phys
require sample

require templates
// require ecs_system_macro

require fio
require daslib/json
require daslib/json_boost

// TODO: create scene with entities 
// TODO: bind createEntity("template", comps_map) and createEntity("template")
// TODO: convert level_1.json to scene

// def custom_cond
//   print(">>>> in the cond")
//   return true

// [es (cond="custom_cond")]
// def test
//   print(">>>> TEST ES")

def get_float2(obj, key)
  unsafe
    let v & = (*obj?[key]).value as _array
    return float2(float(v[0].value as _number), float(v[1].value as _number))

[init]
def init()
  print("[init]")

  create_entity("physics_world")
  create_entity("enemy_spawner")
  create_entity("player_spawner")

  fopen("C:/projects/ecs/sample/data/level_1.json", "r") <| $(f)
    fmap(f) <| $(data)
      print("[load]: level_1.json")
      var error = ""
      let json = read_json(data, error)
      unsafe
        let entities = *(json as _object)?["$entities"]
        for ref in (entities as _array)
          let entity & = (ref.value as _object)
          let templ  & = (*entity?["$template"]).value as _string
          let comps  & = (*entity?["$components"]).value as _object

          print("[load]: {templ}")

          create_entity(templ) <| $(var cm: ComponentsMap)
            for key, refComp in keys(comps), values(comps)
              if key == "pos" || key == "vel"
                let v & = refComp.value as _array
                let x = float(v[0].value as _number)
                let y = float(v[1].value as _number)
                add(cm, key, float2(x, y))
              elif key == "collision_rect"
                let v & = refComp.value as _array
                let x = float(v[0].value as _number)
                let y = float(v[1].value as _number)
                let z = float(v[2].value as _number)
                let w = float(v[3].value as _number)
                add(cm, key, float4(x, y, z, w))
              elif key == "lift_key" || key == "action_key" || key == "key"
                let v & = refComp.value as _string
                add(cm, key, ecs_hash(v))
              elif key == "is_active"
                let v & = refComp.value as _bool
                add(cm, key, v)
              elif key == "dir"
                let v & = refComp.value as _number
                add(cm, key, float(v))
              elif key == "auto_move"
                let v & = refComp.value as _object
                let jump     & = (*v?["jump"]).value as _bool
                let duration & = (*v?["duration"]).value as _number
                let len      & = (*v?["length"]).value as _number
                add(cm, key, [[AutoMove jump=jump, duration=float(duration), length=float(len)]])
              elif key == "collision_shape"
                let arr & = refComp.value as _array
                let v   & = arr[0].value as _object
                let size   = get_float2(v, "size")
                let center = get_float2(v, "center")
                add(cm, key) <|
                  var shape = new CollisionShape
                  add_shape(shape, "box", size, center, 0.f, 1.f, 0.5f)
                  return shape
              else
                panic("[error] Unknown component: {key}")