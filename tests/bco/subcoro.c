#include "shared.h"
#include "../../btest.h"

static btest_suite_t subcoro = {
	.name = "bco/subcoro",
	.init_per_test = init_per_test,
};

bco_static(subcoro_leaf, int id, int steps) {
	bco_vars(int i; int seen;)
	bco_begin
	// `seen` proves each invocation gets a freshly zeroed frame even though
	// every call reuses the same bytes of the parent's stack.
	trace("leaf%d:enter:%d", bco_arg(id), bco_var(seen));
	bco_var(seen) = 1;

	for (bco_var(i) = 0; bco_var(i) < bco_arg(steps); ++bco_var(i)) {
		trace("leaf%d:%d", bco_arg(id), bco_var(i));
		bco_yield();
	}
	bco_end
	trace("leaf%d:cleanup", bco_arg(id));
}

bco_static(subcoro_parent, int calls, int steps) {
	bco_vars(int k;)
	bco_begin
	trace("parent:enter");
	for (bco_var(k) = 0; bco_var(k) < bco_arg(calls); ++bco_var(k)) {
		bco_call(subcoro_leaf, bco_var(k), bco_arg(steps));
	}
	bco_end
	trace("parent:cleanup");
}

BTEST(subcoro, call_transfers_control_and_comes_back) {
	bco_spawn(coro_a(), subcoro_parent, 1, 1);
	drive(coro_a());

	BCO_EXPECT_TRACE(
		"parent:enter leaf0:enter:0 leaf0:0 leaf0:cleanup parent:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// The parent has to suspend while the subcoroutine is mid-flight, so a single
// resume must only advance the innermost coroutine by one step.
BTEST(subcoro, parent_suspends_while_the_sub_runs) {
	bco_spawn(coro_a(), subcoro_parent, 1, 3);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter leaf0:enter:0 leaf0:0");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter leaf0:enter:0 leaf0:0 leaf0:1");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
}

BTEST(subcoro, each_invocation_gets_a_fresh_frame) {
	bco_spawn(coro_a(), subcoro_parent, 3, 1);
	drive(coro_a());

	BCO_EXPECT_TRACE(
		"parent:enter "
		"leaf0:enter:0 leaf0:0 leaf0:cleanup "
		"leaf1:enter:0 leaf1:0 leaf1:cleanup "
		"leaf2:enter:0 leaf2:0 leaf2:cleanup "
		"parent:cleanup"
	);
}

bco_static(subcoro_depth3, int depth) {
	bco_begin
	trace("d%d:enter", bco_arg(depth));
	bco_yield();
	if (bco_arg(depth) < 3) {
		bco_call(subcoro_depth3, bco_arg(depth) + 1);
	}
	bco_end
	trace("d%d:cleanup", bco_arg(depth));
}

BTEST(subcoro, nests_three_deep) {
	bco_spawn(coro_a(), subcoro_depth3, 1);
	drive(coro_a());

	BCO_EXPECT_TRACE(
		"d1:enter d2:enter d3:enter d3:cleanup d2:cleanup d1:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// Terminating the outer coroutine must unwind the whole chain, innermost first.
BTEST(subcoro, terminate_unwinds_the_whole_chain) {
	bco_spawn(coro_a(), subcoro_depth3, 1);
	bco_resume(coro_a());
	bco_resume(coro_a());
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("d1:enter d2:enter d3:enter");

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE(
		"d1:enter d2:enter d3:enter d3:cleanup d2:cleanup d1:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(subcoro_returning_leaf, int id) {
	bco_begin
	trace("leaf%d:enter", bco_arg(id));
	bco_return();
	trace("leaf%d:unreachable", bco_arg(id));
	bco_end
	trace("leaf%d:cleanup", bco_arg(id));
}

bco_static(subcoro_return_parent, int calls) {
	bco_vars(int k;)
	bco_begin
	for (bco_var(k) = 0; bco_var(k) < bco_arg(calls); ++bco_var(k)) {
		bco_call(subcoro_returning_leaf, bco_var(k));
	}
	bco_end
	trace("parent:cleanup");
}

// An early return inside a subcoroutine must only end that subcoroutine and
// let the parent carry on.
BTEST(subcoro, early_return_in_a_sub_resumes_the_parent) {
	bco_spawn(coro_a(), subcoro_return_parent, 2);
	drive(coro_a());

	BCO_EXPECT_TRACE(
		"leaf0:enter leaf0:cleanup leaf1:enter leaf1:cleanup parent:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(subcoro_userdata_leaf, int depth) {
	bco_begin
	trace("d%d:%d", bco_arg(depth), *(const int*)bco_userdata);
	bco_yield();
	bco_end
}

bco_static(subcoro_userdata_parent, int unused) {
	bco_begin
	trace("parent:%d", *(const int*)bco_userdata);
	bco_call(subcoro_userdata_leaf, 1);
	bco_end
}

BTEST(subcoro, userdata_is_inherited_by_subcoroutines) {
	int env = 5;
	bco_spawn(coro_a(), subcoro_userdata_parent, 0);
	bco_set_userdata(coro_a(), &env);

	drive(coro_a());

	BCO_EXPECT_TRACE("parent:5 d1:5");
}

// Retargeting mid-flight has to reach the live subcoroutine too.
BTEST(subcoro, set_userdata_propagates_to_a_live_sub) {
	int env = 5;
	int other_env = 9;
	bco_spawn(coro_a(), subcoro_userdata_parent, 0);
	bco_set_userdata(coro_a(), &env);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:5 d1:5");

	bco_set_userdata(coro_a(), &other_env);
	drive(coro_a());

	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(subcoro_joiner, int unused) {
	bco_begin
	trace("joiner:start");
	bco_join(coro_b());
	trace("joiner:done");
	bco_end
}

// bco_join also works on a coroutine that is not a subcoroutine.
BTEST(subcoro, join_waits_for_an_independent_coroutine) {
	bco_spawn(coro_b(), subcoro_leaf, 9, 2);
	bco_spawn(coro_a(), subcoro_joiner, 0);

	drive(coro_a());

	BCO_EXPECT_TRACE(
		"joiner:start leaf9:enter:0 leaf9:0 leaf9:1 leaf9:cleanup joiner:done"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_b()), BCO_TERMINATED);
}
