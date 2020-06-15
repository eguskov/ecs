options indenting = 2

[export]
def test(var pos: float3&; vel: float3&)
  let dt = 1.f/60.f
  pos += vel * dt
  return true