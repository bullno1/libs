#include <stdio.h>
#include "../../bsfn.h"
#include "../../btest.h"

static btest_suite_t bsfn = {
	.name = "bsfn",
};

static int num_calls = 0;

static int
callback(int arg) {
	num_calls += 1;
	return arg + 1;
}

BTEST(bsfn, stub_call) {
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	BTEST_ASSERT(ctx != NULL);
	bsfn_bind(ctx);

	num_calls = 0;
	int (*stable)(int) = BSFN(callback);
	BTEST_EXPECT(stable != callback);
	BTEST_ASSERT_EQUAL("%d", stable(41), 42);
	BTEST_ASSERT_EQUAL("%d", num_calls, 1);

	bsfn_ctx_destroy(ctx);
}

BTEST(bsfn, same_function_same_stub) {
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	BTEST_ASSERT(ctx != NULL);
	bsfn_bind(ctx);

	// Two wraps of the same function share one identity and thus one stub
	BTEST_EXPECT(BSFN(callback) == BSFN(callback));

	bsfn_ctx_destroy(ctx);
}

static int num_v1_calls = 0;
static int num_v2_calls = 0;

static void
fn_v1(void) {
	num_v1_calls += 1;
}

static void
fn_v2(void) {
	num_v2_calls += 1;
}

// Simulate two loads of the same module by handing bsfn__bind synthetic
// lists: same name, different function address and slot each time.
BTEST(bsfn, stable_across_reload) {
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	BTEST_ASSERT(ctx != NULL);
	num_v1_calls = num_v2_calls = 0;

	bsfn_fn_t slot_v1 = NULL;
	const bsfn_reg_t reg_v1 = {
		.name = "sim.c:callback",
		.fn = (bsfn_fn_t)fn_v1,
		.slot = &slot_v1,
	};
	bsfn__bind(ctx, &reg_v1, &reg_v1 + 1);

	bsfn_fn_t stable = slot_v1;
	BTEST_ASSERT(stable != NULL);
	stable();
	BTEST_ASSERT_EQUAL("%d", num_v1_calls, 1);

	// "Reload": same name, new address, fresh slot
	bsfn_fn_t slot_v2 = NULL;
	const bsfn_reg_t reg_v2 = {
		.name = "sim.c:callback",
		.fn = (bsfn_fn_t)fn_v2,
		.slot = &slot_v2,
	};
	bsfn__bind(ctx, &reg_v2, &reg_v2 + 1);

	// The stable pointer did not change but now reaches the new function
	BTEST_ASSERT(slot_v2 == stable);
	stable();
	BTEST_ASSERT_EQUAL("%d", num_v1_calls, 1);
	BTEST_ASSERT_EQUAL("%d", num_v2_calls, 1);

	// Unbind detaches, a further bind reattaches
	bsfn__unbind(ctx, &reg_v2, &reg_v2 + 1);
	bsfn__bind(ctx, &reg_v2, &reg_v2 + 1);
	stable();
	BTEST_ASSERT_EQUAL("%d", num_v2_calls, 2);

	bsfn_ctx_destroy(ctx);
}

BTEST(bsfn, many_stubs) {
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	BTEST_ASSERT(ctx != NULL);
	num_v1_calls = 0;

	// Force allocation of more than one chunk
	char names[600][16];
	bsfn_fn_t slots[600];
	bsfn_reg_t regs[600];
	for (int i = 0; i < 600; ++i) {
		snprintf(names[i], sizeof(names[i]), "many:%d", i);
		slots[i] = NULL;
		regs[i] = (bsfn_reg_t){
			.name = names[i],
			.fn = (bsfn_fn_t)fn_v1,
			.slot = &slots[i],
		};
	}
	bsfn__bind(ctx, regs, regs + 600);

	for (int i = 0; i < 600; ++i) {
		BTEST_ASSERT(slots[i] != NULL);
		for (int j = 0; j < i; ++j) {
			BTEST_ASSERT(slots[i] != slots[j]);
		}
	}
	slots[0]();
	slots[599]();
	BTEST_ASSERT_EQUAL("%d", num_v1_calls, 2);

	bsfn_ctx_destroy(ctx);
}

#define BSFN_IMPLEMENTATION
#include "../../bsfn.h"
