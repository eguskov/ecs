#include "core.h"

// static ComponentDescriptionDetails<EntityId> _eid;
// int ComponentDescriptionDetails<EntityId>::ID = -1;

REG_COMP_INIT(EntityId, eid);
REG_COMP_INIT(bool, bool);
REG_COMP_INIT(int, int);
REG_COMP_INIT(float, float);
REG_COMP_INIT(glm::vec2, vec2);
REG_COMP_INIT(glm::vec3, vec3);
REG_COMP_INIT(glm::vec4, vec4);
REG_COMP_INIT(eastl::string, string);
REG_COMP_INIT(StringHash, hash_string);
REG_COMP_INIT(Tag, tag);

// REG_COMP_ARR_INIT(glm::vec2, vec2, 2);