#include "common.h"
#include <string.h>

static btest_suite_t unstructured = {
	.name = "bserial/unstructured",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

BTEST(unstructured, number) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	uint64_t u64 = 42;
	BTEST_ASSERT(bserial_any_int(ctx, &u64) == BSERIAL_OK);
	u64 = 0;

	int64_t s64 = -69420;
	BTEST_ASSERT(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	s64 = 0;

	float f32 = 1.5f;
	BTEST_ASSERT(bserial_f32(ctx, &f32) == BSERIAL_OK);
	f32 = 0.f;

	double f64 = 1.5;
	BTEST_ASSERT(bserial_f64(ctx, &f64) == BSERIAL_OK);
	f64 = 0.f;

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	BTEST_ASSERT(bserial_any_int(ctx, &u64) == BSERIAL_OK);
	BTEST_ASSERT(u64 == 42);

	BTEST_ASSERT(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	BTEST_ASSERT(s64 == -69420);

	BTEST_ASSERT(bserial_f32(ctx, &f32) == BSERIAL_OK);
	BTEST_ASSERT(f32 == 1.5f);

	BTEST_ASSERT(bserial_f64(ctx, &f64) == BSERIAL_OK);
	BTEST_ASSERT(f64 == 1.5f);
}

BTEST(unstructured, blob) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	char* str = "Hello world";
	uint64_t len = strlen(str);
	BTEST_ASSERT(bserial_blob(ctx, str, &len) == BSERIAL_OK);

	ctx = common_fixture_make_in_ctx();

	char buf[1024];
	len = sizeof(buf);
	BTEST_ASSERT(bserial_blob(ctx, buf, &len) == BSERIAL_OK);
	BTEST_ASSERT(len == strlen(str));
	BTEST_ASSERT(strncmp(str, buf, len) == 0);
}

static inline bserial_status_t
write_symbol(bserial_ctx_t* ctx, const char** sym) {
	uint64_t len = strlen(*sym);
	return bserial_symbol(ctx, sym, &len);
}

static inline bserial_status_t
read_symbol(bserial_ctx_t* ctx, const char** sym) {
	uint64_t len = 0;
	return bserial_symbol(ctx, sym, &len);
}

BTEST(unstructured, symbol) {
	const char* sym;
	bserial_ctx_t* ctx = common_fixture.out_ctx;

	sym = "Hello";
	BTEST_ASSERT(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "world";
	BTEST_ASSERT(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "world";
	BTEST_ASSERT(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "again";
	BTEST_ASSERT(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "Hello";
	BTEST_ASSERT(write_symbol(ctx, &sym) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();
	const char* a;
	BTEST_ASSERT(read_symbol(ctx, &a) == BSERIAL_OK);
	const char* b;
	BTEST_ASSERT(read_symbol(ctx, &b) == BSERIAL_OK);
	const char* c;
	BTEST_ASSERT(read_symbol(ctx, &c) == BSERIAL_OK);
	const char* d;
	BTEST_ASSERT(read_symbol(ctx, &d) == BSERIAL_OK);
	const char* e;
	BTEST_ASSERT(read_symbol(ctx, &e) == BSERIAL_OK);

	BTEST_ASSERT(a == e);
	BTEST_ASSERT(b == c);
	const char* literal = "again";
	BTEST_ASSERT(strcmp(d, literal) == 0);
}
