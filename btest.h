#ifndef BTEST_H
#define BTEST_H

/// A simple test framework based on autolist

#include "autolist.h"

#ifndef BTEST_LOG_ERROR
#include "blog.h"
#define BTEST_LOG_ERROR(...) BLOG_ERROR(__VA_ARGS__)
#endif

typedef struct {
	const char* name;
	void (*init_per_suite)(void);
	void (*cleanup_per_suite)(void);

	void (*init_per_test)(void);
	void (*cleanup_per_test)(void);
} btest_suite_t;

typedef struct {
	const btest_suite_t* suite;
	const char* name;
	void (*run)(void);
} btest_case_t;

#define BTEST_REGISTER(SUITE, NAME, FN) \
	static void FN(void); \
	AUTOLIST_ENTRY(btest__tests, btest_case_t, btest__case_##SUITE##_##NAME) = { \
		.suite = &SUITE, \
		.name = #NAME, \
		.run = FN, \
	};

#define BTEST(SUITE, NAME) \
	BTEST_REGISTER(SUITE, NAME, SUITE##_##NAME) \
	static void SUITE##_##NAME(void)

#define BTEST_FOREACH(VAR) \
	for (int btest__init_guard = (btest_init(), 0); btest__init_guard < 1; ++btest__init_guard, btest_cleanup()) \
		AUTOLIST_FOREACH(btest__itr, btest__tests) \
			for (const btest_case_t* VAR = btest__itr->value_addr; VAR != NULL; VAR = NULL)

#define BTEST_CHECK(ABORT, COND, ...) \
	do { \
		if (!(COND)) { \
			BTEST_LOG_ERROR(__VA_ARGS__); \
			btest_fail(ABORT); \
		} \
	} while (0)

#define BTEST_ASSERT_EX(COND, MSG, ...) \
	BTEST_CHECK(true, COND, "Assertion failed: %s (" MSG ")", #COND, __VA_ARGS__)

#define BTEST_ASSERT(COND) \
	BTEST_CHECK(true, COND, "Assertion failed: %s", #COND)

#define BTEST_ASSERT_RELATION(FMT, EXP, REL, VALUE) \
	BTEST_ASSERT_EX(EXP REL VALUE, "got "#EXP " == " FMT, EXP)

#define BTEST_ASSERT_EQUAL(FMT, EXP, VALUE) \
	BTEST_ASSERT_RELATION(FMT, EXP, ==, VALUE)

#define BTEST_EXPECT_EX(COND, MSG, ...) \
	BTEST_CHECK(false, COND, "Expectation failed: %s (" MSG ")", #COND, __VA_ARGS__)

#define BTEST_EXPECT(COND) \
	BTEST_CHECK(false, COND, "Expectation failed: %s", #COND)

#define BTEST_EXPECT_RELATION(FMT, EXP, REL, VALUE) \
	BTEST_EXPECT_EX(EXP REL VALUE, "got "#EXP " == " FMT, EXP)

#define BTEST_EXPECT_EQUAL(FMT, EXP, VALUE) \
	BTEST_EXPECT_RELATION(FMT, EXP, ==, VALUE)

AUTOLIST_DECLARE(btest__tests)

void
btest_init(void);

bool
btest_run(const btest_case_t* test);

void
btest_fail(bool abort);

void
btest_cleanup(void);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BTEST_IMPLEMENTATION)
#define BTEST_IMPLEMENTATION
#endif

#ifdef BTEST_IMPLEMENTATION

#include <setjmp.h>

static struct {
	const btest_suite_t* current_suite;
	jmp_buf return_buf;
	bool success;
} btest__ctx = { 0 };

AUTOLIST_IMPL(btest__tests)

void
btest_init(void) {
}

bool
btest_run(const btest_case_t* test) {
	if (test->suite != btest__ctx.current_suite) {
		if (btest__ctx.current_suite && btest__ctx.current_suite->cleanup_per_suite) {
			btest__ctx.current_suite->cleanup_per_suite();
		}

		btest__ctx.current_suite = test->suite;

		if (btest__ctx.current_suite->init_per_suite) {
			btest__ctx.current_suite->init_per_suite();
		}
	}

	if (test->suite->init_per_test != NULL) {
		test->suite->init_per_test();
	}

	btest__ctx.success = true;
	if (setjmp(btest__ctx.return_buf) == 0) { test->run(); }

	if (test->suite->cleanup_per_test != NULL) {
		test->suite->cleanup_per_test();
	}

	return btest__ctx.success;
}

void
btest_fail(bool abort) {
	btest__ctx.success = false;
	if (abort) {
		longjmp(btest__ctx.return_buf, 1);
	}
}

void
btest_cleanup(void) {
	if (btest__ctx.current_suite && btest__ctx.current_suite->cleanup_per_suite) {
		btest__ctx.current_suite->cleanup_per_suite();
	}
	btest__ctx.current_suite = NULL;
}

#ifdef BTEST_INCLUDE_DEFAULT_RUNNER

int
main(int argc, const char* argv[]) {
	const char* suite_filter = NULL;
	const char* test_filter = NULL;
	if (argc > 1) {
		suite_filter = argv[1];
		if (argc > 2) {
			test_filter = argv[2];
		}
	}

	blog_init(&(blog_options_t){
		.current_filename = __FILE__,
		.current_depth_in_project = 1,
	});
	blog_add_file_logger(BLOG_LEVEL_DEBUG, &(blog_file_logger_options_t){
		.file = stderr,
		.with_colors = true,
	});

	int num_tests = 0;
	int num_failed = 0;

	BTEST_FOREACH(test) {
		if (suite_filter && strcmp(suite_filter, test->suite->name) != 0) {
			continue;
		}

		if (test_filter && strcmp(test_filter, test->name) != 0) {
			continue;
		}

		++num_tests;

		BLOG_INFO("---- %s/%s: Running ----", test->suite->name, test->name);
		if (btest_run(test)) {
			BLOG_INFO("---- %s/%s: Passed  ----", test->suite->name, test->name);
		} else {
			BLOG_ERROR("---- %s/%s: Failed  ----", test->suite->name, test->name);
			++num_failed;
		}
	}

	BLOG_INFO("%d/%d tests passed", num_tests - num_failed, num_tests);
	return num_failed;
}

#endif

#endif
