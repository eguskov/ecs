options indenting = 2

require ecs

[generic]
def add(var cmap: ComponentsMap; n: string; v)
  static_if typeinfo(is_pod v) || typeinfo(is_handle v) || typeinfo(is_pointer v)
    _builtin_add(cmap, n, v)
  else
    assert(false)

[generic]
def add(var cmap: ComponentsMap; n: string; s: string)
  _builtin_add(cmap, n, s)

[generic]
def add(var cmap: ComponentsMap; n: string; blk: block<():auto>)
  _builtin_add(cmap, n, invoke(blk))

[generic]
def create_entity(template_name: string)
  create_entity(template_name) <| $(var cm: ComponentsMap) {}
