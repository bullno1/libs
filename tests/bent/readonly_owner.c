// The owning system: the only unit that can write to ro_pos.
// It also implements the component registration.
#define BENT_DEFINE_COMPONENTS
#include "readonly.h"

// Only this file gets a `ro_pos_t*`, as bent_get_mut_ro_pos
BENT_DEFINE_COMP_MUT_GETTER(ro_pos, ro_pos_t)

void
ro_pos_move(bent_world_t* world, bent_t entity, int dx, int dy) {
	ro_pos_t* pos = bent_get_mut_ro_pos(world, entity);
	pos->x += dx;
	pos->y += dy;
}
