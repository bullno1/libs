#ifndef BENT_TEST_SHARED_H
#define BENT_TEST_SHARED_H

#include "../../bent.h"
#include <string.h>

BENT_DECLARE_COMP(basic_component)
BENT_DECLARE_COMP(basic_component2)
BENT_DECLARE_SYS(double_match_system)

static struct {
	bent_world_t* world;
} fixture;

typedef struct {
	int num_adds;
	int num_removes;
	int num_updates;
} simple_system_t;

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
