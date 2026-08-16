#include "shared.h"
#include "../../btest.h"

static int live_resources;

static void
cleanup_init_per_test(void) {
	init_per_test();
	live_resources = 0;
}

static btest_suite_t cleanup = {
	.name = "bco/cleanup",
	.init_per_test = cleanup_init_per_test,
};

/**
 * Acquires a resource, optionally only after a yield.
 *
 * The `acquired` var doubles as the guard the cleanup section reads, which is
 * exactly the pattern that only works because vars are zeroed on first entry.
 */
bco_static(cleanup_resource, int yield_before_acquire) {
	bco_vars(int acquired;)
	bco_begin
	if (bco_arg(yield_before_acquire)) { bco_yield(); }

	bco_var(acquired) = 1;
	++live_resources;
	trace("acquired");

	bco_yield();
	bco_end
	trace("cleanup:%d", bco_var(acquired));
	if (bco_var(acquired)) { --live_resources; }
}

BTEST(cleanup, runs_on_normal_completion) {
	bco_spawn(coro_a(), cleanup_resource, 0);
	drive(coro_a());

	BCO_EXPECT_TRACE("acquired cleanup:1");
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

BTEST(cleanup, runs_on_forced_termination) {
	bco_spawn(coro_a(), cleanup_resource, 0);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("acquired");

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("acquired cleanup:1");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

// A coroutine that never entered its body cannot have acquired anything, so
// running the cleanup section would hand it uninitialized vars.
BTEST(cleanup, is_skipped_when_never_resumed) {
	bco_spawn(coro_a(), cleanup_resource, 0);
	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

// The body started but was terminated before acquiring anything. Cleanup still
// runs, and must see the guard as zero rather than as poisoned memory.
BTEST(cleanup, sees_zeroed_vars_when_terminated_before_acquiring) {
	bco_spawn(coro_a(), cleanup_resource, 1);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("");

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("cleanup:0");
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

BTEST(cleanup, terminate_is_idempotent) {
	bco_spawn(coro_a(), cleanup_resource, 0);
	bco_resume(coro_a());

	bco_terminate(coro_a());
	bco_terminate(coro_a());
	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("acquired cleanup:1");
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

BTEST(cleanup, terminate_after_completion_does_nothing) {
	bco_spawn(coro_a(), cleanup_resource, 0);
	drive(coro_a());
	BCO_EXPECT_TRACE("acquired cleanup:1");

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("acquired cleanup:1");
	BTEST_EXPECT_EQUAL("%d", live_resources, 0);
}

bco_static(cleanup_early_return, int stop_at) {
	bco_vars(int i;)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < 5; ++bco_var(i)) {
		if (bco_var(i) == bco_arg(stop_at)) { bco_return(); }
		trace("%d", bco_var(i));
		bco_yield();
	}
	bco_end
	trace("cleanup:%d", bco_var(i));
}

BTEST(cleanup, early_return_terminates_and_cleans_up) {
	bco_spawn(coro_a(), cleanup_early_return, 2);
	drive(coro_a());

	BCO_EXPECT_TRACE("0 1 cleanup:2");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// bco_return jumps into the cleanup section directly, so no extra resume is
// needed to finish the coroutine off.
BTEST(cleanup, early_return_cleans_up_within_the_same_resume) {
	bco_spawn(coro_a(), cleanup_early_return, 1);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("0");

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("0 cleanup:1");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

BTEST(cleanup, early_return_on_the_very_first_resume) {
	bco_spawn(coro_a(), cleanup_early_return, 0);

	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_TERMINATED);
	BCO_EXPECT_TRACE("cleanup:0");
}

bco_static(cleanup_context, int tag) {
	bco_begin
	bco_yield();
	bco_end
	trace("arg=%d userdata=%d", bco_arg(tag), *(const int*)bco_userdata);
}

// The cleanup section sits outside the body's switch, so it is worth pinning
// down that the usual accessors still resolve there.
BTEST(cleanup, can_read_args_and_userdata) {
	int env = 42;
	bco_spawn(coro_a(), cleanup_context, 7);
	bco_set_userdata(coro_a(), &env);

	drive(coro_a());

	BCO_EXPECT_TRACE("arg=7 userdata=42");
}
