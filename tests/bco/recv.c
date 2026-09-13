#include "shared.h"
#include "../../btest.h"

#include <stdlib.h>

static btest_suite_t recv = {
	.name = "bco/recv",
	.init_per_test = init_per_test,
};

typedef struct { int damage; int kind; } hit_t;
// Same layout as hit_t: a match has to come from the name, not the size
typedef struct { int damage; int kind; } decoy_t;

static hit_t
make_hit(int damage) {
	return (hit_t){ .damage = damage, .kind = 7 };
}

bco_static(receiver, int rounds) {
	bco_vars(int i; hit_t hit;)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(rounds); ++bco_var(i)) {
		trace("wait%d", bco_var(i));
		bco_recv(hit_t, hit);
		trace("got:%d:%d", bco_var(hit).damage, bco_var(hit).kind);
	}
	bco_end
	trace("cleanup");
}

BTEST(recv, send_before_the_receive_is_refused) {
	bco_spawn(coro_a(), receiver, 1);

	// Spawned but never resumed: nothing is waiting yet
	BTEST_EXPECT(!bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
	BCO_EXPECT_TRACE("");

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0");
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
}

BTEST(recv, resume_without_a_value_does_not_enter_the_body) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());

	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_SUSPENDED);
	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_SUSPENDED);

	BCO_EXPECT_TRACE("wait0");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
}

// Sending only delivers. The coroutine has to be resumed to pick the value up.
BTEST(recv, value_is_picked_up_on_the_next_resume) {
	bco_spawn(coro_a(), receiver, 2);
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 3, .kind = 4 }));
	BCO_EXPECT_TRACE("wait0");

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0 got:3:4 wait1");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 5, .kind = 6 }));
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0 got:3:4 wait1 got:5:6 cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

BTEST(recv, a_different_type_is_refused) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());

	BTEST_EXPECT(!bco_send(coro_a(), decoy_t, { .damage = 1, .kind = 1 }));
	BTEST_EXPECT(!bco_send(coro_a(), int, 1));
	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_SUSPENDED);
	BCO_EXPECT_TRACE("wait0");

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
}

BTEST(recv, only_the_first_value_is_accepted) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
	BTEST_EXPECT(!bco_send(coro_a(), hit_t, { .damage = 2, .kind = 2 }));

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0 got:1:1 cleanup");
}

BTEST(recv, every_form_of_value_can_be_sent) {
	bco_spawn(coro_a(), receiver, 3);
	bco_resume(coro_a());

	hit_t variable = { .damage = 1, .kind = 1 };
	BTEST_EXPECT(bco_send(coro_a(), hit_t, variable));
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), hit_t, make_hit(2)));
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), hit_t, (hit_t){ .damage = 3, .kind = 3 }));
	bco_resume(coro_a());

	BCO_EXPECT_TRACE("wait0 got:1:1 wait1 got:2:7 wait2 got:3:3 cleanup");
}

bco_static(int_receiver, int unused) {
	bco_vars(int value;)
	bco_begin
	bco_recv(int, value);
	trace("int:%d", bco_var(value));
	bco_end
}

BTEST(recv, scalars_work) {
	bco_spawn(coro_a(), int_receiver, 0);
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), int, 42));
	bco_resume(coro_a());

	BCO_EXPECT_TRACE("int:42");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(var_receiver, int unused) {
	bco_vars(hit_t hit;)
	bco_begin
	bco_recv(hit_t, hit);
	bco_yield();
	trace("var:%d:%d", bco_var(hit).damage, bco_var(hit).kind);
	bco_end
}

// The whole point of requiring a coroutine variable
BTEST(recv, value_survives_a_later_suspension) {
	bco_spawn(coro_a(), var_receiver, 0);
	bco_resume(coro_a());

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 8, .kind = 9 }));
	drive(coro_a());

	BCO_EXPECT_TRACE("var:8:9");
}

BTEST(recv, send_to_a_finished_coroutine_is_refused) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());
	bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 });
	drive(coro_a());
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);

	BTEST_EXPECT(!bco_send(coro_a(), hit_t, { .damage = 2, .kind = 2 }));

	// Terminated before it ever started
	bco_spawn(coro_b(), receiver, 1);
	bco_terminate(coro_b());
	BTEST_EXPECT(!bco_send(coro_b(), hit_t, { .damage = 2, .kind = 2 }));
}

bco_static(self_sender, int unused) {
	bco_begin
	// A running coroutine cannot be waiting, so this must be refused
	trace("self:%d", bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
	bco_end
}

BTEST(recv, send_to_a_running_coroutine_is_refused) {
	bco_spawn(coro_a(), self_sender, 0);
	drive(coro_a());

	BCO_EXPECT_TRACE("self:0");
}

bco_static(sender, int damage) {
	bco_begin
	trace("send:%d", bco_send(coro_b(), hit_t, { .damage = bco_arg(damage), .kind = 0 }));
	bco_yield();
	bco_end
}

BTEST(recv, a_coroutine_can_send_to_another) {
	bco_spawn(coro_b(), receiver, 1);
	bco_resume(coro_b());

	bco_spawn(coro_a(), sender, 9);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0 send:1");

	// The receiver still needs its own driver to pick the value up
	bco_resume(coro_b());
	BCO_EXPECT_TRACE("wait0 send:1 got:9:0 cleanup");
}

bco_static(parent, int sub_rounds) {
	bco_vars(hit_t hit;)
	bco_begin
	trace("parent:enter");
	bco_call(receiver, bco_arg(sub_rounds));
	trace("parent:wait");
	bco_recv(hit_t, hit);
	trace("parent:got:%d:%d", bco_var(hit).damage, bco_var(hit).kind);
	bco_end
	trace("parent:cleanup");
}

// The host only holds the root handle, so a send has to land in whichever
// coroutine of the chain is actually parked at a receive.
BTEST(recv, delivers_to_the_innermost_subcoroutine) {
	bco_spawn(coro_a(), parent, 1);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter wait0");

	BTEST_EXPECT_EQUAL("%d", bco_resume(coro_a()), BCO_SUSPENDED);
	BCO_EXPECT_TRACE("parent:enter wait0");

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter wait0 got:1:1 cleanup parent:wait");

	// The parent's own receive reuses the same root state after the sub is gone
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 2, .kind = 2 }));
	bco_resume(coro_a());
	BCO_EXPECT_TRACE(
		"parent:enter wait0 got:1:1 cleanup parent:wait parent:got:2:2 parent:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

BTEST(recv, terminate_cancels_a_pending_receive) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("wait0 cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT(!bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));
}

BTEST(recv, terminate_drops_a_delivered_value) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("wait0 cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(cleanup_reader, int unused) {
	bco_vars(hit_t hit;)
	bco_begin
	bco_recv(hit_t, hit);
	trace("body");
	bco_end
	trace("cleanup:%d", bco_var(hit).damage);
}

// The sender writes into the variable at send time, so a value delivered
// right before a terminate is visible to the cleanup section even though the
// body never ran past the receive.
BTEST(recv, delivered_value_is_in_the_variable_during_cleanup) {
	bco_spawn(coro_a(), cleanup_reader, 0);
	bco_resume(coro_a());
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 4, .kind = 0 }));

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("cleanup:4");
}

BTEST(recv, terminate_unwinds_a_waiting_chain) {
	bco_spawn(coro_a(), parent, 1);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter wait0");

	bco_terminate(coro_a());

	BCO_EXPECT_TRACE("parent:enter wait0 cleanup parent:cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// A copy that still pointed at the source's slot would read the right value
// as long as the source is not sent anything, so both are sent something.
BTEST(recv, copy_of_a_waiting_coroutine_waits_on_its_own) {
	bco_spawn(coro_a(), receiver, 1);
	bco_resume(coro_a());

	bco_copy(coro_b(), coro_a());

	BTEST_EXPECT(bco_send(coro_b(), hit_t, { .damage = 2, .kind = 2 }));
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));

	bco_resume(coro_b());
	BCO_EXPECT_TRACE("wait0 got:2:2 cleanup");
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("wait0 got:2:2 cleanup got:1:1 cleanup");
}

BTEST(recv, copy_of_a_waiting_chain_waits_on_its_own) {
	bco_spawn(coro_a(), parent, 1);
	bco_resume(coro_a());

	bco_copy(coro_b(), coro_a());

	BTEST_EXPECT(bco_send(coro_b(), hit_t, { .damage = 2, .kind = 2 }));
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));

	bco_resume(coro_b());
	BCO_EXPECT_TRACE("parent:enter wait0 got:2:2 cleanup parent:wait");
	bco_resume(coro_a());
	BCO_EXPECT_TRACE(
		"parent:enter wait0 got:2:2 cleanup parent:wait got:1:1 cleanup parent:wait"
	);
}

BTEST(recv, heap_allocated_storage) {
	bco_t* coro = malloc(bco_mem_size(64));
	BTEST_ASSERT(coro != NULL);

	bco_spawn(coro, receiver, 1);
	bco_resume(coro);
	BTEST_EXPECT(bco_send(coro, hit_t, { .damage = 1, .kind = 1 }));
	drive(coro);

	BCO_EXPECT_TRACE("wait0 got:1:1 cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro), BCO_TERMINATED);

	free(coro);
}

// --- Hot reload, simulated with the same trampoline as the relocate suite

typedef void (*build_fn_t)(bco_t* coro, void* args);

static build_fn_t worker_build;
static build_fn_t leaf_build;

static int plain_recv_line;

bco_decl_static(worker, int n);
bco_impl(worker) { worker_build(bco__coro, bco__args); }

bco_decl_static(leaf, int n);
bco_impl(leaf) { leaf_build(bco__coro, bco__args); }

bco_static(worker_v1, int n) {
	bco_vars(int i; hit_t hit;)
	bco_yield_points(WAIT_HIT)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(n); ++bco_var(i)) {
		bco_at(WAIT_HIT) bco_recv(hit_t, hit);
		trace("v1:%d", bco_var(hit).damage);
	}
	bco_end
	trace("v1:cleanup");
}

// Names in another order, an extra point and a shifted body
bco_static(worker_v2, int n) {
	bco_vars(int i; hit_t hit;)
	bco_yield_points(WAIT_EXTRA, WAIT_HIT)
	bco_begin
	trace("v2:new-code");
	for (bco_var(i) = 0; bco_var(i) < bco_arg(n); ++bco_var(i)) {
		bco_at(WAIT_EXTRA) bco_yield();
		bco_at(WAIT_HIT) bco_recv(hit_t, hit);
		trace("v2:%d", bco_var(hit).damage);
	}
	bco_end
	trace("v2:cleanup");
}

bco_static(worker_plain, int n) {
	bco_vars(hit_t hit;)
	bco_begin
	plain_recv_line = __LINE__; bco_recv(hit_t, hit);
	trace("plain:%d", bco_var(hit).damage);
	bco_end
}

static void run_worker_v1(bco_t* coro, void* args) { worker_v1(coro, args); }
static void run_worker_v2(bco_t* coro, void* args) { worker_v2(coro, args); }
static void run_worker_plain(bco_t* coro, void* args) { worker_plain(coro, args); }

BTEST(recv, a_plain_receive_blocks_a_reload) {
	worker_build = run_worker_plain;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());

	bco_loc_t blocker = { 0 };
	BTEST_EXPECT(!bco_reloadable(coro_a(), &blocker));
	BTEST_EXPECT(blocker.file != NULL && strcmp(blocker.file, __FILE__) == 0);
	BTEST_EXPECT_EQUAL("%d", plain_recv_line, blocker.line);
}

// The receiver's type name literal is gone with the old build, so the match
// after the swap has to go through the hash.
BTEST(recv, a_named_receive_survives_a_reload) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 2);
	bco_resume(coro_a());
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v2;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	BTEST_EXPECT(!bco_send(coro_a(), decoy_t, { .damage = 1, .kind = 1 }));
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 5, .kind = 0 }));
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("v2:5");

	// The second round runs entirely in the new build
	bco_resume(coro_a());  // WAIT_EXTRA -> WAIT_HIT
	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 6, .kind = 0 }));
	drive(coro_a());
	BCO_EXPECT_TRACE("v2:5 v2:6 v2:cleanup");
}

bco_static(leaf_v1, int n) {
	bco_vars(hit_t hit;)
	bco_yield_points(LEAF_RECV)
	bco_begin
	trace("leaf1:enter");
	bco_at(LEAF_RECV) bco_recv(hit_t, hit);
	trace("leaf1:%d", bco_var(hit).damage);
	bco_end
	trace("leaf1:cleanup");
}

// LEAF_RECV no longer exists
bco_static(leaf_v2, int n) {
	bco_yield_points(LEAF_OTHER)
	bco_begin
	bco_at(LEAF_OTHER) bco_yield();
	bco_end
	trace("leaf2:cleanup");
}

bco_static(parent_v1, int n) {
	bco_yield_points(WAIT_LEAF)
	bco_begin
	trace("parent:enter");
	bco_at(WAIT_LEAF) bco_call(leaf, 0);
	trace("parent:after");
	bco_end
	trace("parent:cleanup");
}

static void run_leaf_v1(bco_t* coro, void* args) { leaf_v1(coro, args); }
static void run_leaf_v2(bco_t* coro, void* args) { leaf_v2(coro, args); }
static void run_parent_v1(bco_t* coro, void* args) { parent_v1(coro, args); }

// The parent carries on after a reload killed the waiting leaf. The wait it
// registered at the root must go with it, or a later send would write into
// a slot the parent's stack has since reused.
BTEST(recv, a_leaf_terminated_by_a_reload_cancels_its_receive) {
	leaf_build = run_leaf_v1;
	worker_build = run_parent_v1;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());
	BCO_EXPECT_TRACE("parent:enter leaf1:enter");

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	leaf_build = run_leaf_v2;
	BTEST_EXPECT(!bco_reload_end(coro_a()));
	BCO_EXPECT_TRACE("parent:enter leaf1:enter leaf2:cleanup");

	BTEST_EXPECT(!bco_send(coro_a(), hit_t, { .damage = 1, .kind = 1 }));

	drive(coro_a());
	BCO_EXPECT_TRACE(
		"parent:enter leaf1:enter leaf2:cleanup parent:after parent:cleanup"
	);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

// --- The type itself changes across the reload

// A bigger type with the same name in the new build. The macro stringifies its
// argument before expansion, so `hit_t` below still registers as "hit_t"
// while sizeof and the type check see the new layout.
//
// The variable holding it grows with it. The sender writes straight into the
// variable, so the grown value lands exactly where the new build reads it.
typedef struct { int damage; int kind; int extra; char tag[16]; } hit_v2_t;

#define hit_t hit_v2_t

bco_static(worker_v1_grown, int n) {
	bco_vars(int i; hit_t hit;)
	bco_yield_points(WAIT_HIT)
	bco_begin
	bco_at(WAIT_HIT) bco_recv(hit_t, hit);
	trace("grown:%d:%d:%d:%s", bco_var(hit).damage, bco_var(hit).kind, bco_var(hit).extra, bco_var(hit).tag);
	bco_end
	trace("grown:cleanup");
}

static void run_worker_v1_grown(bco_t* coro, void* args) { worker_v1_grown(coro, args); }

// The receiver was reloaded along with the type, so the new build of the
// sender and the new build of the receiver agree on the layout. The old size
// recorded by the receive must not get in the way.
BTEST(recv, a_received_type_may_change_size_across_a_reload) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 1);
	bco_resume(coro_a());

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v1_grown;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	BTEST_EXPECT(bco_send(coro_a(), hit_t, { .damage = 5, .kind = 6, .extra = 7, .tag = "new" }));

	// A copy taken now has to carry the whole grown value along
	bco_copy(coro_b(), coro_a());
	bco_resume(coro_b());
	BCO_EXPECT_TRACE("grown:5:6:7:new grown:cleanup");

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("grown:5:6:7:new grown:cleanup grown:5:6:7:new grown:cleanup");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

#undef hit_t
