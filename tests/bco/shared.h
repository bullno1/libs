#ifndef BCO_TEST_SHARED_H
#define BCO_TEST_SHARED_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../bco.h"

/**
 * Storage for the coroutines under test.
 *
 * It is deliberately poisoned before every test so that reading uninitialized
 * coroutine memory shows up as garbage instead of accidentally correct zeroes.
 */
#define BCO_TEST_BUF_SIZE 1024

/// Upper bound on resumes before a test gives up, so a stuck coroutine fails
/// instead of hanging the whole run.
#define BCO_TEST_MAX_STEPS 32

static struct {
	_Alignas(bco_align_t) char mem_a[BCO_TEST_BUF_SIZE];
	_Alignas(bco_align_t) char mem_b[BCO_TEST_BUF_SIZE];

	char trace[512];
	size_t trace_len;
} fixture;

static inline bco_t*
coro_a(void) {
	return (bco_t*)fixture.mem_a;
}

static inline bco_t*
coro_b(void) {
	return (bco_t*)fixture.mem_b;
}

/**
 * Append an event to the trace.
 *
 * Coroutine tests are mostly about *ordering*: which body ran, how far it got,
 * and when cleanup fired. Recording events into one string lets a test state
 * the entire expected schedule in a single comparison.
 */
static inline void
trace(const char* fmt, ...) {
	size_t space = sizeof(fixture.trace) - fixture.trace_len;
	if (space <= 1) { return; }

	if (fixture.trace_len > 0) {
		fixture.trace[fixture.trace_len++] = ' ';
		--space;
		if (space <= 1) { return; }
	}

	va_list args;
	va_start(args, fmt);
	int written = vsnprintf(fixture.trace + fixture.trace_len, space, fmt, args);
	va_end(args);

	if (written > 0) {
		size_t len = (size_t)written;
		fixture.trace_len += len < space ? len : space - 1;
	}
	fixture.trace[fixture.trace_len] = '\0';
}

static inline const char*
trace_str(void) {
	return fixture.trace;
}

#define BCO_EXPECT_TRACE(EXPECTED) \
	BTEST_EXPECT_EX( \
		strcmp(trace_str(), (EXPECTED)) == 0, \
		"trace is \"%s\", expected \"%s\"", trace_str(), (EXPECTED) \
	)

/// Resume until termination, returning the number of resumes it took
static inline int
drive(bco_t* coro) {
	int steps = 0;
	while (bco_status(coro) != BCO_TERMINATED && steps < BCO_TEST_MAX_STEPS) {
		bco_resume(coro);
		++steps;
	}
	return steps;
}

static inline void
init_per_test(void) {
	memset(fixture.mem_a, 0x5A, sizeof(fixture.mem_a));
	memset(fixture.mem_b, 0x5A, sizeof(fixture.mem_b));
	fixture.trace[0] = '\0';
	fixture.trace_len = 0;
}

#endif
