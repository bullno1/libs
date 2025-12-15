#ifndef BSV_TEST_SHARED_H
#define BSV_TEST_SHARED_H

#include <stdlib.h>

#define BSV_MEM
#include "../../bsv.h"

static struct {
	bsv_mem_in_t mem_in;
	bsv_mem_out_t mem_out;
} fixture;

static inline void
init_per_test(void) {
	bsv_mem_init_out(&fixture.mem_out, NULL);
}

static inline void
cleanup_per_test(void) {
	free(fixture.mem_out.mem);
	fixture.mem_out.mem = NULL;
}

static inline bsv_out_t*
bsv_mem_out(void) {
	free(fixture.mem_out.mem);
	fixture.mem_out.mem = NULL;

	return bsv_mem_init_out(&fixture.mem_out, NULL);
}

static inline bsv_in_t*
bsv_mem_in(void) {
	return bsv_mem_init_in(&fixture.mem_in, fixture.mem_out.mem, fixture.mem_out.len);
}

#endif
