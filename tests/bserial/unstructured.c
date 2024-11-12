#include "common.h"
#include <assert.h>

static suite_t unstructured = {
	.name = "unstructured",
	.init = common_fixture_init,
	.cleanup = common_fixture_cleanup,
};

TEST(unstructured, number) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	uint64_t u64 = 42;
	assert(bserial_any_int(ctx, &u64) == BSERIAL_OK);
	u64 = 0;

	uint64_t s64 = -42;
	assert(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	s64 = 0;

	float f32 = 1.5f;
	assert(bserial_f32(ctx, &f32) == BSERIAL_OK);
	f32 = 0.f;

	double f64 = 1.5;
	assert(bserial_f64(ctx, &f64) == BSERIAL_OK);
	f64 = 0.f;

	ctx = common_fixture_make_in_ctx();

	assert(bserial_any_int(ctx, &u64) == BSERIAL_OK);
	assert(u64 == 42);

	assert(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	assert(s64 == -42);

	assert(bserial_f32(ctx, &f32) == BSERIAL_OK);
	assert(f32 == 1.5f);

	assert(bserial_f64(ctx, &f64) == BSERIAL_OK);
	assert(f64 == 1.5f);
}
