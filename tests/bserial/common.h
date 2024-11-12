#ifndef BSERIAL_TEST_COMMON_H
#define BSERIAL_TEST_COMMON_H

#define BSERIAL_MEM
#include "../../autolist.h"
#include "../../bserial.h"

typedef struct {
	const char* name;
	void (*init)(void);
	void (*cleanup)(void);
} suite_t;

typedef struct {
	suite_t* suite;
	const char* name;
	void (*run)(void* fixture);
} test_t;

#define TEST(SUITE, NAME) \
	static void SUITE##_##NAME(void* fixture); \
	AUTOLIST_ENTRY(bserial_test, test_t, test_##SUITE##_##NAME) = { \
		.suite = &SUITE, \
		.name = #NAME, \
		.run = SUITE##_##NAME, \
	}; \
	static void SUITE##_##NAME(void* fixture)

typedef struct {
	bserial_mem_out_t mem_out;
	bserial_mem_in_t mem_in;
	bserial_ctx_config_t ctx_config;
	bserial_ctx_t* out_ctx;
	bserial_ctx_t* in_ctx;
} common_fixture_t;

extern common_fixture_t common_fixture;

void
common_fixture_init(void);

void
common_fixture_cleanup(void);

bserial_ctx_t*
common_fixture_make_in_ctx(void);

#endif
