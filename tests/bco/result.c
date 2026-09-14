#include "shared.h"
#include "../../btest.h"

#include <stdint.h>

static btest_suite_t result = {
	.name = "bco/result",
	.init_per_test = init_per_test,
};

typedef struct { int damage; int kind; } hit_t;
typedef enum { NONE, FIRE = 7 } element_t;

static hit_t
make_hit(int damage) {
	return (hit_t){ .damage = damage, .kind = 9 };
}

/// Whether `p` points inside the given fixture buffer
static int
inside(const void* p, const char* buf) {
	uintptr_t addr = (uintptr_t)p;
	uintptr_t base = (uintptr_t)buf;
	return addr >= base && addr < base + BCO_TEST_BUF_SIZE;
}

bco_static(hit_t, roller, int strength) {
	bco_begin
	trace("roll:%d", bco_arg(strength));
	bco_yield();
	bco_return({ .damage = bco_arg(strength) * 2, .kind = 1 });
	bco_end
	trace("roll:cleanup");
}

BTEST(result, host_reads_the_returned_value) {
	bco_spawn(coro_a(), roller, 3);
	BTEST_EXPECT(bco_result(coro_a(), roller) == NULL);
	bco_resume(coro_a());
	BTEST_EXPECT(bco_result(coro_a(), roller) == NULL);

	drive(coro_a());

	hit_t* hit = bco_result(coro_a(), roller);
	BTEST_ASSERT(hit != NULL);
	BTEST_EXPECT_EQUAL("%d", hit->damage, 6);
	BTEST_EXPECT_EQUAL("%d", hit->kind, 1);
	// The value is stored before the cleanup section runs
	BCO_EXPECT_TRACE("roll:3 roll:cleanup");
}

bco_static(hit_t, no_value, int unused) {
	bco_begin
	bco_yield();
	bco_end
	trace("cleanup");
}

BTEST(result, falling_off_the_end_leaves_no_value) {
	bco_spawn(coro_a(), no_value, 0);
	drive(coro_a());

	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT(bco_result(coro_a(), no_value) == NULL);
}

bco_static(hit_t, plain_return, int unused) {
	bco_begin
	bco_return();
	bco_end
}

// A returning coroutine may still leave early without a value
BTEST(result, plain_return_leaves_no_value) {
	bco_spawn(coro_a(), plain_return, 0);
	drive(coro_a());

	BTEST_EXPECT(bco_result(coro_a(), plain_return) == NULL);
}

BTEST(result, terminate_leaves_no_value) {
	bco_spawn(coro_a(), roller, 3);
	bco_resume(coro_a());
	bco_terminate(coro_a());
	BTEST_EXPECT(bco_result(coro_a(), roller) == NULL);

	// Never started
	bco_spawn(coro_b(), roller, 3);
	bco_terminate(coro_b());
	BTEST_EXPECT(bco_result(coro_b(), roller) == NULL);
}

bco_static(hit_t, forms, int which) {
	bco_vars(hit_t stored;)
	bco_begin
	bco_var(stored) = (hit_t){ .damage = 1, .kind = 1 };
	if (bco_arg(which) == 0) { bco_return(bco_var(stored)); }
	if (bco_arg(which) == 1) { bco_return(make_hit(2)); }
	if (bco_arg(which) == 2) { bco_return((hit_t){ .damage = 3, .kind = 3 }); }
	bco_return({ .damage = 4, .kind = 4 });
	bco_end
}

BTEST(result, every_form_of_value_can_be_returned) {
	for (int which = 0; which < 4; ++which) {
		bco_spawn(coro_a(), forms, which);
		drive(coro_a());

		hit_t* hit = bco_result(coro_a(), forms);
		BTEST_ASSERT(hit != NULL);
		BTEST_EXPECT_EQUAL("%d", hit->damage, which + 1);
	}
}

bco_static(int, sum_to, int n) {
	bco_vars(int i; int total;)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(n); ++bco_var(i)) {
		bco_var(total) += bco_var(i);
		bco_yield();
	}
	bco_return(bco_var(total));
	bco_end
}

bco_static(element_t, pick_element, int unused) {
	bco_begin
	bco_return(FIRE);
	bco_end
}

bco_static(void*, own_handle, int unused) {
	bco_begin
	bco_return(bco_self);
	bco_end
}

BTEST(result, scalars_enums_and_pointers) {
	bco_spawn(coro_a(), sum_to, 4);
	drive(coro_a());
	int* total = bco_result(coro_a(), sum_to);
	BTEST_ASSERT(total != NULL);
	BTEST_EXPECT_EQUAL("%d", *total, 0 + 1 + 2 + 3);

	bco_spawn(coro_a(), pick_element, 0);
	drive(coro_a());
	element_t* element = bco_result(coro_a(), pick_element);
	BTEST_ASSERT(element != NULL);
	BTEST_EXPECT_EQUAL("%d", *element, FIRE);

	bco_spawn(coro_a(), own_handle, 0);
	drive(coro_a());
	void** handle = bco_result(coro_a(), own_handle);
	BTEST_ASSERT(handle != NULL);
	BTEST_EXPECT_EQUAL("%p", *handle, (void*)coro_a());
}

bco_static(void, consumer, int damage) {
	bco_begin
	trace("consume:%d", bco_arg(damage));
	bco_yield();
	bco_end
}

bco_static(void, attacker, int strength) {
	bco_vars(hit_t hit;)
	bco_begin
	bco_call_result(hit, roller, bco_arg(strength));
	// The motivating case: the value feeds the next call
	bco_call(consumer, bco_var(hit).damage);
	bco_end
	trace("attacker:cleanup:%d", bco_var(hit).damage);
}

BTEST(result, parent_takes_the_value_and_passes_it_on) {
	bco_spawn(coro_a(), attacker, 4);
	drive(coro_a());

	BCO_EXPECT_TRACE("roll:4 roll:cleanup consume:8 attacker:cleanup:8");
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
}

bco_static(hit_t, ignorer, int unused) {
	bco_begin
	bco_call(roller, 1);
	bco_end
}

// A plain bco_call discards the value, so it must not surface as the
// parent's own result when the parent ends without one.
BTEST(result, ignored_subcoroutine_value_does_not_reach_the_host) {
	bco_spawn(coro_a(), ignorer, 0);
	drive(coro_a());

	BTEST_EXPECT(bco_result(coro_a(), ignorer) == NULL);
}

bco_static(hit_t, wrapper, int unused) {
	bco_begin
	bco_call(roller, 1);
	bco_return({ .damage = 100, .kind = 0 });
	bco_end
}

BTEST(result, parent_returns_its_own_value_after_a_call) {
	bco_spawn(coro_a(), wrapper, 0);
	drive(coro_a());

	hit_t* hit = bco_result(coro_a(), wrapper);
	BTEST_ASSERT(hit != NULL);
	BTEST_EXPECT_EQUAL("%d", hit->damage, 100);
}

bco_static(int, inner, int n) {
	bco_begin
	bco_yield();
	bco_return(bco_arg(n) + 1);
	bco_end
}

bco_static(int, middle, int n) {
	bco_vars(int v;)
	bco_begin
	bco_call_result(v, inner, bco_arg(n));
	bco_return(bco_var(v) * 10);
	bco_end
}

bco_static(void, outer, int n) {
	bco_vars(int v;)
	bco_begin
	bco_call_result(v, middle, bco_arg(n));
	trace("outer:%d", bco_var(v));
	bco_end
}

// Each level takes its own subcoroutine's value before returning its own
BTEST(result, nested_result_calls) {
	bco_spawn(coro_a(), outer, 2);
	drive(coro_a());

	BCO_EXPECT_TRACE("outer:30");
}

BTEST(result, copy_carries_the_value) {
	bco_spawn(coro_a(), roller, 5);
	drive(coro_a());

	bco_copy(coro_b(), coro_a());

	hit_t* a = bco_result(coro_a(), roller);
	hit_t* b = bco_result(coro_b(), roller);
	BTEST_ASSERT(a != NULL && b != NULL);
	BTEST_EXPECT_EQUAL("%d", b->damage, 10);
	BTEST_EXPECT(inside(a, fixture.mem_a));
	BTEST_EXPECT(inside(b, fixture.mem_b));
}

BTEST(result, copy_taken_mid_call_completes_on_its_own) {
	bco_spawn(coro_a(), attacker, 2);
	bco_resume(coro_a());  // roller parked at its yield
	BCO_EXPECT_TRACE("roll:2");

	bco_copy(coro_b(), coro_a());

	drive(coro_b());
	BCO_EXPECT_TRACE("roll:2 roll:cleanup consume:4 attacker:cleanup:4");
	drive(coro_a());
	BCO_EXPECT_TRACE(
		"roll:2 roll:cleanup consume:4 attacker:cleanup:4 "
		"roll:cleanup consume:4 attacker:cleanup:4"
	);
}

// --- Hot reload, simulated with the same trampoline as the relocate suite

typedef void (*build_fn_t)(bco_t* coro, void* args);

static build_fn_t worker_build;
static build_fn_t parent_build;

bco_decl_static(hit_t, worker, int n);
bco_impl(worker) { worker_build(bco__coro, bco__args); }

bco_decl_static(void, parent, int n);
bco_impl(parent) { parent_build(bco__coro, bco__args); }

bco_static(hit_t, worker_v1, int n) {
	bco_yield_points(WAIT)
	bco_begin
	trace("v1");
	bco_at(WAIT) bco_yield();
	bco_return({ .damage = 1, .kind = 1 });
	bco_end
}

bco_static(hit_t, worker_v2, int n) {
	bco_yield_points(EXTRA, WAIT)
	bco_begin
	trace("v2");
	bco_at(EXTRA) bco_yield();
	bco_at(WAIT) bco_yield();
	bco_return({ .damage = 2, .kind = 2 });
	bco_end
}

bco_static(void, parent_v1, int n) {
	bco_vars(hit_t hit;)
	bco_yield_points(WAIT_SUB)
	bco_begin
	bco_at(WAIT_SUB) bco_call_result(hit, worker, 0);
	trace("parent:%d", bco_var(hit).damage);
	bco_end
}

static void run_worker_v1(bco_t* coro, void* args) { worker_v1(coro, args); }
static void run_worker_v2(bco_t* coro, void* args) { worker_v2(coro, args); }
static void run_parent_v1(bco_t* coro, void* args) { parent_v1(coro, args); }

BTEST(result, value_returned_by_the_new_build_reaches_the_host) {
	worker_build = run_worker_v1;
	bco_spawn(coro_a(), worker, 0);
	bco_resume(coro_a());  // WAIT

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v2;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	drive(coro_a());
	hit_t* hit = bco_result(coro_a(), worker);
	BTEST_ASSERT(hit != NULL);
	BTEST_EXPECT_EQUAL("%d", hit->damage, 2);
	BCO_EXPECT_TRACE("v1");
}

// bco_call_result contains exactly one suspension, so bco_at applies to it
// like it does to bco_call.
BTEST(result, call_result_is_relocatable) {
	worker_build = run_worker_v1;
	parent_build = run_parent_v1;
	bco_spawn(coro_a(), parent, 0);
	bco_resume(coro_a());  // parent at WAIT_SUB, worker at WAIT
	BTEST_EXPECT(bco_reloadable(coro_a(), NULL));

	BTEST_EXPECT(bco_reload_begin(coro_a()));
	worker_build = run_worker_v2;
	BTEST_EXPECT(bco_reload_end(coro_a()));

	drive(coro_a());
	BCO_EXPECT_TRACE("v1 parent:2");
}
