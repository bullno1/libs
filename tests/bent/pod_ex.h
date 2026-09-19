#ifndef BENT_TEST_POD_EX_H
#define BENT_TEST_POD_EX_H

#include "../../bent.h"

// In a header shared by every system
typedef struct { int x, y; } ex_pos_t;
typedef struct { bent_t target; } ex_link_t;
typedef struct { int frame; } ex_cache_t;

// Mutable helpers, never saved
BENT_POD_COMP_EX(ex_pos, ex_pos_t, RW, TRANSIENT)

// Mutable helpers, saved through bent_serialize_ex_link which this declares
BENT_POD_COMP_EX(ex_link, ex_link_t, RW, SERIALIZED)

// Const helpers, saved through bent_serialize_ex_cache
BENT_POD_COMP_EX(ex_cache, ex_cache_t, RO, SERIALIZED)

#endif
