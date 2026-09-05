#define BSFN_NO_RELOAD
#include "../../bsfn.h"
#include "../../btest.h"

static btest_suite_t bsfn_no_reload = {
	.name = "bsfn_no_reload",
};

static int num_calls = 0;

static void
callback(void) {
	num_calls += 1;
}

BTEST(bsfn_no_reload, passthrough) {
	// The whole API must compile to no-ops
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	bsfn_bind(ctx);

	// BSFN is the wrapped function itself
	BTEST_ASSERT(BSFN(callback) == callback);
	num_calls = 0;
	BSFN(callback)();
	BTEST_ASSERT_EQUAL("%d", num_calls, 1);

	bsfn_unbind(ctx);
	bsfn_ctx_destroy(ctx);
}
