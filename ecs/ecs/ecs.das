options indenting = 2

require ecs

[generic]
def add(var cmap: ComponentsMap; n: string; v)
  static_if typeinfo(is_pod v) || typeinfo(is_handle v) || typeinfo(is_pointer v) || typeinfo(is_string v)
    _builtin_add(cmap, n, v)
  else
    try
      _builtin_add(cmap, n, invoke(v))
    recover
      assert(false)

[generic]
def create_entity(template_name: string)
  create_entity(template_name) <| $(var cm: ComponentsMap) {}
