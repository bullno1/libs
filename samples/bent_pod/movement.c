// The owner of the position component: the only unit that can write to it
#include "components.h"

//!                                                                             [BENT_DEFINE_COMP_MUT_GETTER]
// Only this file gets a `position_t*`, as bent_get_mut_position
BENT_DEFINE_COMP_MUT_GETTER(position, position_t)

void
movement_move(bent_world_t* world, bent_t entity, int dx, int dy) {
	position_t* pos = bent_get_mut_position(world, entity);
	if (pos == NULL) { return; }

	pos->x += dx;
	pos->y += dy;
}
//!                                                                             [BENT_DEFINE_COMP_MUT_GETTER]
