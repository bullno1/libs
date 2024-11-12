#include "common.h"
#include <assert.h>
#include <string.h>

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

	int64_t s64 = -69420;
	assert(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	s64 = 0;

	float f32 = 1.5f;
	assert(bserial_f32(ctx, &f32) == BSERIAL_OK);
	f32 = 0.f;

	double f64 = 1.5;
	assert(bserial_f64(ctx, &f64) == BSERIAL_OK);
	f64 = 0.f;

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	assert(bserial_any_int(ctx, &u64) == BSERIAL_OK);
	assert(u64 == 42);

	assert(bserial_any_int(ctx, &s64) == BSERIAL_OK);
	assert(s64 == -69420);

	assert(bserial_f32(ctx, &f32) == BSERIAL_OK);
	assert(f32 == 1.5f);

	assert(bserial_f64(ctx, &f64) == BSERIAL_OK);
	assert(f64 == 1.5f);
}

TEST(unstructured, blob) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	char* str = "Hello world";
	uint64_t len = strlen(str);
	assert(bserial_blob(ctx, str, &len) == BSERIAL_OK);

	ctx = common_fixture_make_in_ctx();

	char buf[1024];
	len = sizeof(buf);
	assert(bserial_blob(ctx, buf, &len) == BSERIAL_OK);
	assert(len == strlen(str));
	assert(strncmp(str, buf, len) == 0);
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

TEST(unstructured, symbol) {
	const char* sym;
	bserial_ctx_t* ctx = common_fixture.out_ctx;

	sym = "Hello";
	assert(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "world";
	assert(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "world";
	assert(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "again";
	assert(write_symbol(ctx, &sym) == BSERIAL_OK);
	sym = "Hello";
	assert(write_symbol(ctx, &sym) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();
	const char* a;
	assert(read_symbol(ctx, &a) == BSERIAL_OK);
	const char* b;
	assert(read_symbol(ctx, &b) == BSERIAL_OK);
	const char* c;
	assert(read_symbol(ctx, &c) == BSERIAL_OK);
	const char* d;
	assert(read_symbol(ctx, &d) == BSERIAL_OK);
	const char* e;
	assert(read_symbol(ctx, &e) == BSERIAL_OK);

	assert(a == e);
	assert(b == c);
	const char* literal = "again";
	assert(strcmp(d, literal) == 0);
}
