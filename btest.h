#ifndef BTEST_H
#define BTEST_H

/// A simple test framework based on autolist

#include "autolist.h"

#ifndef BTEST_LOG_ERROR
#include <blog.h>
#define BTEST_LOG_ERROR(...) BLOG_ERROR(__VA_ARGS__)
#endif

typedef struct {
	const char* name;
	void (*init)(void);
	void (*cleanup)(void);
} btest_suite_t;

typedef struct {
	btest_suite_t* suite;
	const char* name;
	void (*run)(void);
} btest_case_t;

#define BTEST(SUITE, NAME) \
	static void SUITE##_##NAME(void); \
	AUTOLIST_ENTRY(btest__tests, btest_case_t, btest__case_##SUITE##_##NAME) = { \
		.suite = &SUITE, \
		.name = #NAME, \
		.run = SUITE##_##NAME, \
	}; \
	static void SUITE##_##NAME(void)

#define BTEST_FOREACH(VAR) \
	AUTOLIST_FOREACH(btest__itr, btest__tests) \
		for (const autolist_entry_t* btest__entry = *btest__itr; btest__entry != NULL; btest__entry = NULL) \
			for (btest_case_t* VAR = btest__entry->value_addr; VAR != NULL; VAR = NULL)

#define BTEST_ASSERT(COND) \
	do { \
		if (!(COND)) { \
			BTEST_LOG_ERROR("Assertion failed: %s", #COND); \
			btest_fail(); \
		} \
	} while (0)

bool
btest_run(btest_case_t* test);

void
btest_fail(void);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BTEST_IMPLEMENTATION)
#define BTEST_IMPLEMENTATION
#endif

#ifdef BTEST_IMPLEMENTATION

#include <setjmp.h>

static struct {
	jmp_buf return_buf;
	bool success;
} btest__ctx;

AUTOLIST_DECLARE(btest__tests)

bool
btest_run(btest_case_t* test) {
	if (test->suite->init != NULL) {
		test->suite->init();
	}

	btest__ctx.success = true;
	if (setjmp(btest__ctx.return_buf) == 0) { test->run(); }

	if (test->suite->cleanup != NULL) {
		test->suite->cleanup();
	}

	return btest__ctx.success;
}

void
btest_fail(void) {
	btest__ctx.success = false;
	longjmp(btest__ctx.return_buf, 1);
}

#endif
