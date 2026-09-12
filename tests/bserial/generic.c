#include "common.h"
#include <string.h>

static btest_suite_t generic = {
	.name = "bserial/generic",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

BTEST(generic, any_int_fundamental_types) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	char c = 'x';
	signed char sc = -5;
	unsigned char uc = 250;
	short sh = -300;
	unsigned short ush = 60000;
	int i = -70000;
	unsigned int ui = 4000000000u;
	long l = -1234567890123L;
	unsigned long ul = 12345678901234UL;
	long long ll = -9000000000000000000LL;
	unsigned long long ull = 18000000000000000000ULL;
	size_t sz = 987654321;
	int64_t i64 = INT64_MIN;
	uint64_t u64 = UINT64_MAX;

	BTEST_ASSERT(bserial_any_int(ctx, &c) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sc) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &uc) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sh) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ush) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &i) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ui) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &l) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ul) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ll) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ull) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sz) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &i64) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &u64) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	char c2 = 0; signed char sc2 = 0; unsigned char uc2 = 0;
	short sh2 = 0; unsigned short ush2 = 0; int i2 = 0; unsigned int ui2 = 0;
	long l2 = 0; unsigned long ul2 = 0; long long ll2 = 0; unsigned long long ull2 = 0;
	size_t sz2 = 0; int64_t i642 = 0; uint64_t u642 = 0;
	BTEST_ASSERT(bserial_any_int(ctx, &c2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sc2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &uc2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sh2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ush2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &i2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ui2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &l2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ul2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ll2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &ull2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &sz2) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &i642) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &u642) == BSERIAL_OK);

	BTEST_ASSERT(c2 == c);
	BTEST_ASSERT(sc2 == sc);
	BTEST_ASSERT(uc2 == uc);
	BTEST_ASSERT(sh2 == sh);
	BTEST_ASSERT(ush2 == ush);
	BTEST_ASSERT(i2 == i);
	BTEST_ASSERT(ui2 == ui);
	BTEST_ASSERT(l2 == l);
	BTEST_ASSERT(ul2 == ul);
	BTEST_ASSERT(ll2 == ll);
	BTEST_ASSERT(ull2 == ull);
	BTEST_ASSERT(sz2 == sz);
	BTEST_ASSERT(i642 == i64);
	BTEST_ASSERT(u642 == u64);
}

BTEST(generic, any_int_range_check) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int big = 300;
	int negative = -1;
	BTEST_ASSERT(bserial_any_int(ctx, &big) == BSERIAL_OK);
	BTEST_ASSERT(bserial_any_int(ctx, &negative) == BSERIAL_OK);

	// 300 does not fit in uint8_t
	ctx = common_fixture_make_in_ctx();
	uint8_t u8 = 0;
	BTEST_ASSERT(bserial_any_int(ctx, &u8) == BSERIAL_MALFORMED);

	// 300 fits in int16_t but -1 does not fit in uint16_t
	ctx = common_fixture_make_in_ctx();
	int16_t i16 = 0;
	uint16_t u16 = 0;
	BTEST_ASSERT(bserial_any_int(ctx, &i16) == BSERIAL_OK);
	BTEST_ASSERT(i16 == 300);
	BTEST_ASSERT(bserial_any_int(ctx, &u16) == BSERIAL_MALFORMED);
}

BTEST(generic, len_types) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int values[] = { 7, 8, 9 };

	int array_len = 3;
	BTEST_ASSERT(bserial_array(ctx, &array_len) == BSERIAL_OK);
	for (int i = 0; i < array_len; ++i) {
		BTEST_ASSERT(bserial_any_int(ctx, &values[i]) == BSERIAL_OK);
	}

	size_t blob_len = 5;
	BTEST_ASSERT(bserial_blob_header(ctx, &blob_len) == BSERIAL_OK);
	BTEST_ASSERT(bserial_blob_body(ctx, "hello") == BSERIAL_OK);

	unsigned char whole_len = 3;
	BTEST_ASSERT(bserial_blob(ctx, "abc", &whole_len) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	// Read the same lengths back into different integer types
	long long array_len2 = 0;
	BTEST_ASSERT(bserial_array(ctx, &array_len2) == BSERIAL_OK);
	BTEST_ASSERT(array_len2 == 3);
	int values2[3] = { 0 };
	for (int i = 0; i < array_len2; ++i) {
		BTEST_ASSERT(bserial_any_int(ctx, &values2[i]) == BSERIAL_OK);
	}
	BTEST_ASSERT(memcmp(values, values2, sizeof(values)) == 0);

	short blob_len2 = 0;
	BTEST_ASSERT(bserial_blob_header(ctx, &blob_len2) == BSERIAL_OK);
	BTEST_ASSERT(blob_len2 == 5);
	char text[8] = { 0 };
	BTEST_ASSERT(bserial_blob_body(ctx, text) == BSERIAL_OK);
	BTEST_ASSERT(strcmp(text, "hello") == 0);

	char abc[8] = { 0 };
	uint64_t whole_len2 = sizeof(abc);  // Capacity in, actual length out
	BTEST_ASSERT(bserial_blob(ctx, abc, &whole_len2) == BSERIAL_OK);
	BTEST_ASSERT(whole_len2 == 3);
	BTEST_ASSERT(strcmp(abc, "abc") == 0);
}

BTEST(generic, len_negative_on_write) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int len = -1;
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_MALFORMED);
}

BTEST(generic, len_overflow_on_read) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	uint64_t len = 300;
	BTEST_ASSERT(bserial_array_u64(ctx, &len) == BSERIAL_OK);
	for (uint64_t i = 0; i < len; ++i) {
		int v = (int)i;
		BTEST_ASSERT(bserial_any_int(ctx, &v) == BSERIAL_OK);
	}

	ctx = common_fixture_make_in_ctx();
	uint8_t small = 0;
	BTEST_ASSERT(bserial_array(ctx, &small) == BSERIAL_MALFORMED);
}
