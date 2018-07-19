#include "ecs/ecs.h"

#include "components/test.component.h"
#include "components/position.component.h"

void test_printer(const UpdateStage &stage,
  EntityId eid,
  const TestComponent &test,
  const PositionComponent &position)
{
  std::cout << "test_printer(" << eid.handle << ")" << std::endl;
  std::cout << "TEST (" << test.a << ", " << test.b << ")" << std::endl;
}
REG_SYS_2(test_printer, "t", "p");