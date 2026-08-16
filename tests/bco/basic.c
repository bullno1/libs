#include "shared.h"
#include "../../btest.h"

#include <stdint.h>
#include <stdlib.h>

static btest_suite_t basic = {
	.name = "bco/basic",
	.init_per_test = init_per_test,
};

bco_static(counter, int from, int to) {
	bco_vars(int i;)
	bco_begin
	for (bco_var(i) = bco_arg(from); bco_var(i) < bco_arg(to); ++bco_var(i)) {
		trace("%d", bco_var(i));
		bco_yield();
	}
	bco_end
	trace("done");
}

BTEST(basic, spawn_leaves_it_suspended) {
	bco_spawn(coro_a(), counter, 0, 3);

	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
	// Spawning must not run any of the body
	BCO_EXPECT_TRACE("");
}

// A yield must return control to the caller. Without it the whole body would
// run inside the first bco_resume.
BTEST(basic, yield_suspends_the_body) {
	bco_spawn(coro_a(), counter, 0, 3);

	bco_resume(coro_a());
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
	BCO_EXPECT_TRACE("0");

	bco_resume(coro_a());
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
	BCO_EXPECT_TRACE("0 1");
}

BTEST(basic, runs_to_completion) {
	bco_spawn(coro_a(), counter, 0, 3);

	int steps = drive(coro_a());

	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BCO_EXPECT_TRACE("0 1 2 done");
	// 3 yields + 1 resume that falls off the end
	BTEST_EXPECT_EQUAL("%d", steps, 4);
}

BTEST(basic, resume_after_termination_is_a_noop) {
	bco_spawn(coro_a(), counter, 0, 1);
	drive(coro_a());
	BCO_EXPECT_TRACE("0 done");

	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_TERMINATED);
	BCO_EXPECT_TRACE("0 done");
}

// An empty body still has to reach the cleanup section and terminate
bco_static(basic_empty, int unused) {
	bco_begin
	bco_end
	trace("cleanup");
}

BTEST(basic, empty_body_terminates_in_one_resume) {
	bco_spawn(coro_a(), basic_empty, 0);

	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_TERMINATED);
	BCO_EXPECT_TRACE("cleanup");
}

bco_static(basic_no_args) {
	bco_begin
	trace("ran");
	bco_yield();
	bco_end
}

BTEST(basic, coroutine_without_arguments) {
	bco_spawn(coro_a(), basic_no_args);
	drive(coro_a());

	BCO_EXPECT_TRACE("ran");
}

bco_static(basic_no_vars, int a) {
	bco_begin
	trace("%d", bco_arg(a));
	bco_yield();
	trace("%d", bco_arg(a));
	bco_end
}

BTEST(basic, coroutine_without_vars) {
	bco_spawn(coro_a(), basic_no_vars, 7);
	drive(coro_a());

	BCO_EXPECT_TRACE("7 7");
}

// Arguments are copied into the coroutine's own storage at spawn time, so the
// caller's variable going out of scope or changing must not be observable.
BTEST(basic, arguments_are_copied_by_value) {
	int caller_owned = 11;
	bco_spawn(coro_a(), basic_no_vars, caller_owned);

	caller_owned = 999;
	drive(coro_a());

	BCO_EXPECT_TRACE("11 11");
}

// Regression: the frame used to be re-allocated on every resume, which both
// lost the values and walked the stack pointer off the end of the buffer.
static const void* first_var_address;
static const void* second_var_address;

bco_static(basic_var_address, int unused) {
	bco_vars(int i;)
	bco_begin
	first_var_address = (const void*)&bco_var(i);
	bco_yield();
	second_var_address = (const void*)&bco_var(i);
	bco_end
}

BTEST(basic, vars_keep_the_same_address_across_resumes) {
	first_var_address = NULL;
	second_var_address = NULL;

	bco_spawn(coro_a(), basic_var_address, 0);
	drive(coro_a());

	BTEST_EXPECT(first_var_address != NULL);
	BTEST_EXPECT_EQUAL("%p", second_var_address, first_var_address);
}

bco_static(basic_var_zeroing, int unused) {
	bco_vars(int i; void* p; char c;)
	bco_begin
	trace("i=%d p=%s c=%d",
		bco_var(i),
		bco_var(p) == NULL ? "null" : "nonnull",
		(int)bco_var(c));
	bco_yield();
	bco_end
}

// The storage is poisoned by init_per_test, so a var reading as zero can only
// come from the library zeroing the frame on first entry.
BTEST(basic, vars_are_zero_initialized_on_first_entry) {
	bco_spawn(coro_a(), basic_var_zeroing, 0);
	drive(coro_a());

	BCO_EXPECT_TRACE("i=0 p=null c=0");
}

bco_static(overaligned, int unused) {
	bco_vars(_Alignas(16) char blob[16]; int tag;)
	bco_begin
	bco_var(tag) = 0x1234;
	trace("aligned=%d", ((uintptr_t)bco_var(blob) % 16u) == 0);
	bco_yield();
	trace("tag=%#x", bco_var(tag));
	bco_end
}

BTEST(basic, overaligned_vars_are_honoured) {
	bco_spawn(coro_a(), overaligned, 0);
	drive(coro_a());

	BCO_EXPECT_TRACE("aligned=1 tag=0x1234");
}

BTEST(basic, heap_allocated_storage) {
	bco_t* coro = malloc(bco_mem_size(256));
	BTEST_ASSERT(coro != NULL);

	bco_spawn(coro, counter, 0, 2);
	drive(coro);

	BTEST_EXPECT_EQUAL("%d", bco_status(coro), BCO_TERMINATED);
	BCO_EXPECT_TRACE("0 1 done");

	free(coro);
}

BTEST(basic, two_coroutines_are_independent) {
	bco_spawn(coro_a(), counter, 0, 2);
	bco_spawn(coro_b(), counter, 10, 12);

	bco_resume(coro_a());
	bco_resume(coro_b());
	bco_resume(coro_a());
	bco_resume(coro_b());

	BCO_EXPECT_TRACE("0 10 1 11");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_b()), BCO_SUSPENDED);
}
