#ifndef BENT_TEST_READONLY_H
#define BENT_TEST_READONLY_H

#include "../../bent.h"

//! [BENT_POD_COMP_EX]
// In a header shared by every system
typedef struct { int x, y; } ro_pos_t;

// Both bent_add_ro_pos and bent_get_ro_pos return `const ro_pos_t*`
BENT_POD_COMP_EX(ro_pos, ro_pos_t, RO, TRANSIENT)

// Other systems ask the owner to write on their behalf
void
ro_pos_move(bent_world_t* world, bent_t entity, int dx, int dy);
//! [BENT_POD_COMP_EX]

#endif
