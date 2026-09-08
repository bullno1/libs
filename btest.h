#ifndef BTEST_H
#define BTEST_H

/**
 * @file
 * @brief A simple test framework based on @ref autolist.h.
 *
 * Tests are declared with @ref BTEST and automatically registered.
 * A custom runner can iterate over them with @ref BTEST_FOREACH.
 *
 * When a debugger is attached, a failed check will break into it at the site of
 * the failure.
 * This makes the state that caused the failure available for inspection.
 * See @ref BTEST_BREAK and @ref btest_debugger_attached.
 *
 * Debugger detection is supported on Windows and Linux.
 * On every other platform, no debugger is ever reported and the behaviour is
 * unchanged.
 */

#include "autolist.h"

/*! Customizable log function for failed checks */
#ifndef BTEST_LOG_ERROR
#include "blog.h"
#define BTEST_LOG_ERROR(...) BLOG_ERROR(__VA_ARGS__)
#endif

/*! A test suite */
typedef struct {
	/*! Name of the suite */
	const char* name;
	/*! (Optional) Called once before the first test of the suite */
	void (*init_per_suite)(void);
	/*! (Optional) Called once after the last test of the suite */
	void (*cleanup_per_suite)(void);

	/*! (Optional) Called before each test */
	void (*init_per_test)(void);
	/*! (Optional) Called after each test */
	void (*cleanup_per_test)(void);
} btest_suite_t;

/*! A test case */
typedef struct {
	/*! The suite this test belongs to */
	const btest_suite_t* suite;
	/*! Name of the test */
	const char* name;
	/*! The test function */
	void (*run)(void);
} btest_case_t;

/**
 * Register an existing function as a test.
 *
 * @param SUITE the suite (@ref btest_suite_t) this test belongs to
 * @param NAME name of the test
 * @param FN the test function
 *
 * @hideinitializer
 */
#define BTEST_REGISTER(SUITE, NAME, FN) \
	static void FN(void); \
	AUTOLIST_ENTRY(btest__tests, btest_case_t, btest__case_##SUITE##_##NAME) = { \
		.suite = &SUITE, \
		.name = #NAME, \
		.run = FN, \
	};

/**
 * Declare and register a test function.
 *
 * @param SUITE the suite (@ref btest_suite_t) this test belongs to
 * @param NAME name of the test
 *
 * @hideinitializer
 */
#define BTEST(SUITE, NAME) \
	BTEST_REGISTER(SUITE, NAME, SUITE##_##NAME) \
	static void SUITE##_##NAME(void)

/**
 * Iterate over all registered tests.
 *
 * @param VAR name of the iterator variable, of type `const btest_case_t*`
 *
 * @hideinitializer
 */
#define BTEST_FOREACH(VAR) \
	for (int btest__init_guard = (btest_init(), 0); btest__init_guard < 1; ++btest__init_guard, btest_cleanup()) \
		AUTOLIST_FOREACH(btest__itr, btest__tests) \
			for (const btest_case_t* VAR = btest__itr->value_addr; VAR != NULL; VAR = NULL)

/**
 * Break into the attached debugger.
 *
 * Define it as `((void)0)` to never break, even under a debugger.
 *
 * @hideinitializer
 */
#ifndef BTEST_BREAK
#	if defined(_MSC_VER)
#		include <intrin.h>
#		define BTEST_BREAK() __debugbreak()
#	elif defined(__has_builtin)
#		if __has_builtin(__builtin_debugtrap)
#			define BTEST_BREAK() __builtin_debugtrap()
#		endif
#	endif
#endif

/* No dedicated builtin: trap by hand */
#ifndef BTEST_BREAK
#	if defined(__i386__) || defined(__x86_64__)
#		define BTEST_BREAK() __asm__ volatile("int3")
#	elif defined(__aarch64__)
#		define BTEST_BREAK() __asm__ volatile("brk #0")
#	else
#		include <signal.h>
#		define BTEST_BREAK() raise(SIGTRAP)
#	endif
#endif

/**
 * Check a condition and log a custom message on failure.
 *
 * When a debugger is attached, this breaks into it (@ref BTEST_BREAK) before
 * failing the test, regardless of @p ABORT.
 *
 * @param ABORT whether to abort the test on failure
 * @param COND the condition to check
 * @param ... printf-style format string and arguments for the failure message
 *
 * @hideinitializer
 */
#define BTEST_CHECK(ABORT, COND, ...) \
	do { \
		if (!(COND)) { \
			BTEST_LOG_ERROR(__VA_ARGS__); \
			if (btest_debugger_attached()) { BTEST_BREAK(); } \
			btest_fail(ABORT); \
		} \
	} while (0)

/**
 * Assert a condition with a custom message, aborting the test on failure.
 *
 * @param COND the condition to check
 * @param MSG printf-style format string for extra context
 * @param ... arguments for the format string
 *
 * @hideinitializer
 */
#define BTEST_ASSERT_EX(COND, MSG, ...) \
	BTEST_CHECK(true, COND, "Assertion failed: %s (" MSG ")", #COND, __VA_ARGS__)

/**
 * Assert a condition, aborting the test on failure.
 *
 * @param COND the condition to check
 *
 * @hideinitializer
 */
#define BTEST_ASSERT(COND) \
	BTEST_CHECK(true, COND, "Assertion failed: %s", #COND)

/**
 * Assert a relation between an expression and a value, aborting the test on failure.
 *
 * @param FMT printf-style format specifier for the expression's value
 * @param EXP the expression to check
 * @param REL the relational operator (e.g: `==`, `<`...)
 * @param VALUE the value to compare against
 *
 * @hideinitializer
 */
#define BTEST_ASSERT_RELATION(FMT, EXP, REL, VALUE) \
	BTEST_ASSERT_EX(EXP REL VALUE, "got "#EXP " == " FMT, EXP)

/**
 * Assert that an expression equals a value, aborting the test on failure.
 *
 * @param FMT printf-style format specifier for the expression's value
 * @param EXP the expression to check
 * @param VALUE the expected value
 *
 * @hideinitializer
 */
#define BTEST_ASSERT_EQUAL(FMT, EXP, VALUE) \
	BTEST_ASSERT_RELATION(FMT, EXP, ==, VALUE)

/**
 * Same as @ref BTEST_ASSERT_EX but the test continues on failure.
 *
 * @param COND the condition to check
 * @param MSG printf-style format string for extra context
 * @param ... arguments for the format string
 *
 * @hideinitializer
 */
#define BTEST_EXPECT_EX(COND, MSG, ...) \
	BTEST_CHECK(false, COND, "Expectation failed: %s (" MSG ")", #COND, __VA_ARGS__)

/**
 * Same as @ref BTEST_ASSERT but the test continues on failure.
 *
 * @param COND the condition to check
 *
 * @hideinitializer
 */
#define BTEST_EXPECT(COND) \
	BTEST_CHECK(false, COND, "Expectation failed: %s", #COND)

/**
 * Same as @ref BTEST_ASSERT_RELATION but the test continues on failure.
 *
 * @param FMT printf-style format specifier for the expression's value
 * @param EXP the expression to check
 * @param REL the relational operator (e.g: `==`, `<`...)
 * @param VALUE the value to compare against
 *
 * @hideinitializer
 */
#define BTEST_EXPECT_RELATION(FMT, EXP, REL, VALUE) \
	BTEST_EXPECT_EX(EXP REL VALUE, "got "#EXP " == " FMT, EXP)

/**
 * Same as @ref BTEST_ASSERT_EQUAL but the test continues on failure.
 *
 * @param FMT printf-style format specifier for the expression's value
 * @param EXP the expression to check
 * @param VALUE the expected value
 *
 * @hideinitializer
 */
#define BTEST_EXPECT_EQUAL(FMT, EXP, VALUE) \
	BTEST_EXPECT_RELATION(FMT, EXP, ==, VALUE)

AUTOLIST_DECLARE(btest__tests)

/*! Initialize the test framework, called by @ref BTEST_FOREACH */
void
btest_init(void);

/*! Run a single test and return whether it passed */
bool
btest_run(const btest_case_t* test);

/*! Mark the current test as failed, optionally aborting it */
void
btest_fail(bool abort);

/*! Check whether a debugger is currently attached to this process. */
bool
btest_debugger_attached(void);

/*! Clean up the test framework, called by @ref BTEST_FOREACH */
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

#if defined(_WIN32)
// Windows {{{

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool
btest_debugger_attached(void) {
	return IsDebuggerPresent() != 0;
}

// }}}
#elif defined(__linux__)
// Linux {{{

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

bool
btest_debugger_attached(void) {
	// TracerPid is non-zero while a process is being traced.
	// It sits near the top of the file so a single read always covers it.
	int fd = open("/proc/self/status", O_RDONLY);
	if (fd < 0) { return false; }

	char status[1024];
	ssize_t num_bytes_read = read(fd, status, sizeof(status) - 1);
	close(fd);
	if (num_bytes_read <= 0) { return false; }
	status[num_bytes_read] = '\0';

	const char* tracer_pid = strstr(status, "TracerPid:");
	if (tracer_pid == NULL) { return false; }

	tracer_pid += sizeof("TracerPid:") - 1;
	while (*tracer_pid == ' ' || *tracer_pid == '\t') { ++tracer_pid; }

	return *tracer_pid != '0';
}

// }}}
#else
// Unsupported {{{

bool
btest_debugger_attached(void) {
	return false;
}

// }}}
#endif

void
btest_cleanup(void) {
	if (btest__ctx.current_suite && btest__ctx.current_suite->cleanup_per_suite) {
		btest__ctx.current_suite->cleanup_per_suite();
	}
	btest__ctx.current_suite = NULL;
}

#ifdef BTEST_INCLUDE_DEFAULT_RUNNER

#ifndef BTEST_LOG_DEPTH
#define BTEST_LOG_DEPTH 2 /* deps/blibs/btest.h */
#endif

#include <string.h>

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
		.current_depth_in_project = BTEST_LOG_DEPTH,
	});
	blog_add_file_logger(BLOG_LEVEL_TRACE, &(blog_file_logger_options_t){
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
