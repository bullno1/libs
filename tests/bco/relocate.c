#include "shared.h"
#include "../../btest.h"

static btest_suite_t relocate = {
	.name = "bco/relocate",
	.init_per_test = init_per_test,
};

/*
 * Simulating a hot reload.
 *
 * Making a function pointer survive a code swap is the job of another library.
 * Here it is stood in for by a trampoline: the coroutine the tests spawn is
 * `worker`, whose body just forwards to whichever build is "loaded".
 * The builds share the argument layout, as a real reload would require.
 */
typedef void (*build_fn_t)(bco_t* coro, void* args);

static build_fn_t worker_build;
static build_fn_t leaf_build;

// Lines of the plain suspensions, recorded right before them so the tests can
// check that bco_reloadable points at the right place.
static int worker_v1_yield_line;
static int leaf_v1_yield_line;
static int parent_line_call_line;

#define EXPECT_BLOCKED_AT(CORO, LINE) \
	do { \
		bco_loc_t at = { 0 }; \
		BTEST_EXPECT(!bco_reloadable(CORO, &at)); \
		BTEST_EXPECT_EX( \
			at.file != NULL && strcmp(at.file, __FILE__) == 0, \
			"blocker file is \"%s\", expected \"%s\"", at.file != NULL ? at.file : "(null)", __FILE__ \
		); \
		BTEST_EXPECT_EQUAL("%d", LINE, at.line); \
	} while (0)

bco_decl_static(worker, int n);
bco_impl(worker) { worker_build(bco__coro, bco__args); }

bco_decl_static(leaf, int n);
bco_impl(leaf) { leaf_build(bco__coro, bco__args); }

// --- Build 1

bco_static(worker_v1, int n) {
	bco_vars(int i;)
	bco_yield_points(WAIT_A, WAIT_B)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(n); ++bco_var(i)) {
		trace("v1:a%d", bco_var(i));
		bco_at(WAIT_A) bco_yield();
		trace("v1:b%d", bco_var(i));
		bco_at(WAIT_B) bco_yield();
		trace("v1:line%d", bco_var(i));
		worker_v1_yield_line = __LINE__; bco_yield();
	}
	bco_end
	trace("v1:cleanup");
}

// --- Build 2: names declared in another order, body shifted by new lines and
// a new point. WAIT_B must still land in front of "v2:c".

bco_static(worker_v2, int n) {
	bco_vars(int i;)
	bco_yield_points(WAIT_C, WAIT_B, WAIT_A)
	bco_begin
	trace("v2:new-code");
	for (bco_var(i) = 0; bco_var(i) < bco_arg(n); ++bco_var(i)) {
		trace("v2:a%d", bco_var(i));
		bco_at(WAIT_A) bco_yield();
		trace("v2:b%d", bco_var(i));
		bco_at(WAIT_B) bco_yield();
		trace("v2:c%d", bco_var(i));
		bco_at(WAIT_C) bco_yield();
	}
	bco_end
	trace("v2:cleanup");
}

// --- Build 3: WAIT_B no longer exists

bco_static(worker_v3, int n) {
	bco_yield_points(WAIT_A)
	bco_begin
	bco_at(WAIT_A) bco_yield();
	bco_end
	trace("v3:cleanup");
}

static void run_worker_v1(bco_t* coro, void* args) { worker_v1(coro, args); }
static void run_worker_v2(bco_t* coro, void* args) { worker_v2(coro, args); }
static void run_worker_v3(bco_t* coro, void* args) { worker_v3(coro, args); }

BTEST(relocate, fresh_and_terminated_coroutines_are_relocatable) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
	BTEST_EXPECT(bco_reload_begin(coro_a()));
	BTEST_EXPECT(bco_reload_end(coro_a()));

	drive(coro_a());
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
}

BTEST(relocate, only_named_points_are_relocatable) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);

	bco_resume(coro_a());  // WAIT_A
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
	bco_resume(coro_a());  // WAIT_B
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
	bco_resume(coro_a());  // plain bco_yield
	BTEST_EXPECT(!bco_reloadable(coro_a(), NULL));
}

BTEST(relocate, reports_where_the_plain_yield_is) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);

	// The blocker is left alone while the coroutine is reloadable
	bco_loc_t at = { .file = "untouched", .line = -1 };
	BTEST_EXPECT(bco_reloadable(coro_a(), &at));
	bco_resume(coro_a());  // WAIT_A
	BTEST_EXPECT(bco_reloadable(coro_a(), &at));
	BTEST_EXPECT(strcmp(at.file, "untouched") == 0);
	BTEST_EXPECT_EQUAL("%d", -1, at.line);

	bco_resume(coro_a());  // WAIT_B
	bco_resume(coro_a());  // plain bco_yield
	EXPECT_BLOCKED_AT(coro_a(), worker_v1_yield_line);
}

BTEST(relocate, refused_begin_leaves_the_coroutine_runnable) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);
	bco_resume(coro_a());
	bco_resume(coro_a());
	bco_resume(coro_a());  // plain bco_yield

	BTEST_EXPECT(!bco_reload_begin(coro_a()));

	drive(coro_a());
	BCO_EXPECT_TRACE("v1:a0 v1:b0 v1:line0 v1:cleanup");
}

BTEST(relocate, resumes_in_the_new_build_by_name) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 2);
	for (int i = 0; i < 5; ++i) { bco_resume(coro_a()); }
	BCO_EXPECT_TRACE("v1:a0 v1:b0 v1:line0 v1:a1 v1:b1");

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v2;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	// Continues right after WAIT_B of the new build, with its variables intact
	drive(coro_a());
	BCO_EXPECT_TRACE("v1:a0 v1:b0 v1:line0 v1:a1 v1:b1 v2:c1 v2:cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

BTEST(relocate, a_vanished_point_terminates_with_cleanup) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);
	bco_resume(coro_a());
	bco_resume(coro_a());  // WAIT_B

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v3;
	BTEST_EXPECT(!bco_reload_end(coro_a()));

	BCO_EXPECT_TRACE("v1:a0 v1:b0 v3:cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// --- Subcoroutines: the whole chain has to be at named points and every link
// is relocated with its own build's table.

bco_static(leaf_v1, int n) {
	bco_yield_points(LEAF_WAIT)
	bco_begin
	trace("leaf1:enter");
	bco_at(LEAF_WAIT) bco_yield();
	trace("leaf1:line");
	leaf_v1_yield_line = __LINE__; bco_yield();
	bco_end
	trace("leaf1:cleanup");
}

bco_static(leaf_v2, int n) {
	bco_yield_points(LEAF_EXTRA, LEAF_WAIT)
	bco_begin
	trace("leaf2:enter");
	bco_at(LEAF_EXTRA) bco_yield();
	bco_at(LEAF_WAIT) bco_yield();
	trace("leaf2:after");
	bco_end
	trace("leaf2:cleanup");
}

static void run_leaf_v1(bco_t* coro, void* args) { leaf_v1(coro, args); }
static void run_leaf_v2(bco_t* coro, void* args) { leaf_v2(coro, args); }

bco_static(parent_v1, int n) {
	bco_yield_points(WAIT_LEAF)
	bco_begin
	trace("parent:enter");
	bco_at(WAIT_LEAF) bco_call(leaf, 0);
	trace("parent:after");
	bco_end
	trace("parent:cleanup");
}

bco_static(parent_line, int n) {
	bco_begin
	parent_line_call_line = __LINE__; bco_call(leaf, 0);
	bco_end
}

static void run_parent_v1(bco_t* coro, void* args) { parent_v1(coro, args); }
static void run_parent_line(bco_t* coro, void* args) { parent_line(coro, args); }

BTEST(relocate, every_link_of_the_chain_must_be_named) {
	leaf_build = run_leaf_v1;

	worker_build = run_parent_v1;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());  // parent at WAIT_LEAF, leaf at LEAF_WAIT
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
	bco_resume(coro_a());  // leaf at a plain yield
	EXPECT_BLOCKED_AT(coro_a(), leaf_v1_yield_line);

	worker_build = run_parent_line;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());  // parent at a plain bco_call, leaf at LEAF_WAIT
	EXPECT_BLOCKED_AT(coro_a(), parent_line_call_line);

	bco_resume(coro_a());  // parent still at the plain bco_call, leaf at a plain yield
	// The outermost blocker is reported first
	EXPECT_BLOCKED_AT(coro_a(), parent_line_call_line);
}

BTEST(relocate, relocates_the_whole_chain) {
	leaf_build = run_leaf_v1;
	worker_build = run_parent_v1;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter leaf1:enter");

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	leaf_build = run_leaf_v2;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	drive(coro_a());
	BCO_EXPECT_TRACE(
		"parent:enter leaf1:enter leaf2:after leaf2:cleanup parent:after parent:cleanup"
	);
}

bco_static(joiner_v1, int n) {
	bco_yield_points(WAIT_OTHER)
	bco_begin
	trace("joiner:start");
	bco_at(WAIT_OTHER) bco_join(coro_b());
	trace("joiner:done");
	bco_end
}

static void run_joiner_v1(bco_t* coro, void* args) { joiner_v1(coro, args); }

BTEST(relocate, join_is_relocatable) {
	leaf_build = run_leaf_v1;
	bco_spawn(coro_b(), leaf, 0);
	worker_build = run_joiner_v1;
	bco_spawn(coro_a(), worker, 0);

	bco_resume(coro_a());
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));
	BTEST_EXPECT(bco_reload_begin(coro_a()));
	BTEST_EXPECT(bco_reload_end(coro_a()));

	drive(coro_a());
	BCO_EXPECT_TRACE("joiner:start leaf1:enter leaf1:line leaf1:cleanup joiner:done");
}
