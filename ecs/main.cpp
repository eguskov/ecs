#include "stdafx.h"

#include <stdint.h>

#include <vector>
#include <array>
#include <string>
#include <bitset>
#include <algorithm>
#include <iostream>

#include "ecs/ecs.h"

int main()
{
  EntityManager mgr;
  EntityId eid1 = mgr.createEntity("test");
  EntityId eid2 = mgr.createEntity("test");
  EntityId eid3 = mgr.createEntity("test1");

  mgr.tick(UpdateStage{ 0.5f });
  mgr.tick();

  return 0;
}

