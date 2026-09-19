#ifndef BENT_TEST_SHARED_H
#define BENT_TEST_SHARED_H

#include "../../bent.h"
#include <string.h>

BENT_DECLARE_COMP(basic_component)
BENT_DECLARE_COMP(basic_component2)
BENT_DECLARE_SYS(double_match_system)

// See shared.c
extern bool bent_test_tolerate_asserts;
extern int bent_test_num_failed_asserts;

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

static inline int
count_query(bent_world_t* world, bent_query_t query) {
	int count = 0;
	BENT_FOREACH_QUERY(entity, world, query) {
		(void)entity;
		++count;
	}
	return count;
}

#endif
