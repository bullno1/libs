#ifndef BENT_TEST_SHARED_H
#define BENT_TEST_SHARED_H

#include "../../bent.h"
#include <string.h>

BENT_DECLARE_COMP(basic_component)
BENT_DECLARE_COMP(basic_component2)

static struct {
	bent_world_t* world;
} fixture;

static inline void
init_per_test(void) {
	memset(&fixture, 0, sizeof(fixture));
	bent_init(&fixture.world, NULL);
}

static inline void
cleanup_per_test(void) {
	bent_cleanup(&fixture.world);
}

#endif
