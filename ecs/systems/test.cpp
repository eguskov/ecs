#include "ecs/ecs.h"

#include "components/test.component.h"
#include "components/position.component.h"

#include "stages/update.stage.h"

struct FooComponent
{
  float x = 1.f;
  float y = 2.f;

  bool set(const JValue &value)
  {
    assert(value.HasMember("x"));
    x = value["x"].GetFloat();
    assert(value.HasMember("y"));
    y = value["y"].GetFloat();
    return true;
  }
};
REG_COMP_AND_INIT(FooComponent, foo);

void foo_sys(const UpdateStage &stage,
  const FooComponent &foo)
{
  // It works!
  // std::cout << "FOO (" << foo.x << ", " << foo.y << ")" << std::endl;
}
REG_SYS_1(foo_sys, "foo");