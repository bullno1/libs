#include "common.h"
#include <string.h>

static btest_suite_t array = {
	.name = "bserial/array",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

BTEST(array, simple) {
	int numbers[] = { 1, 2, 3 };
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	uint64_t len = sizeof(numbers) / sizeof(numbers[0]);
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_OK);
	for (uint64_t i = 0; i < len; ++i) {
		BTEST_ASSERT(bserial_any_int(ctx, &numbers[i]) == BSERIAL_OK);
	}

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	int buff[16];
	len = sizeof(buff) / sizeof(buff[0]);
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_OK);
	BTEST_ASSERT(len == 3);
	for (uint64_t i = 0; i < len; ++i) {
		BTEST_ASSERT(bserial_any_int(ctx, &buff[i]) == BSERIAL_OK);
	}

	BTEST_ASSERT(memcmp(numbers, buff, sizeof(buff[0]) * len) == 0);
}

typedef struct {
	int len;
	int values[7];
} int_array_t;

typedef struct {
	int len;
	int_array_t values[7];
} nested_array_t;

static bserial_status_t
serialize_int_array(bserial_ctx_t* ctx, int_array_t* int_array) {
	uint64_t len = (uint64_t)int_array->len;
	BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
	if (len > 7) { return BSERIAL_MALFORMED; }
	int_array->len = (int)len;

	for (int i = 0; i < len; ++i) {
		BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &int_array->values[i]));
	}

	return BSERIAL_OK;
}

static bserial_status_t
serialize_nested_array(bserial_ctx_t* ctx, nested_array_t* nested_array) {
	uint64_t len = (uint64_t)nested_array->len;
	BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
	if (len > 7) { return BSERIAL_MALFORMED; }
	nested_array->len = (int)len;
	for (int i = 0; i < len; ++i) {
		BSERIAL_CHECK_STATUS(serialize_int_array(ctx, &nested_array->values[i]));
	}

	return BSERIAL_OK;
}

BTEST(array, nested) {
	nested_array_t src_array = {
		.len = 2,
		.values = {
			[0] = {
				.len = 2,
				.values = { 1, 2 },
			},
			[1] = {
				.len = 3,
				.values = { 3, 4, 5 },
			},
		},
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_nested_array(ctx, &src_array) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	nested_array_t array2 = { 0 };
	BTEST_ASSERT(serialize_nested_array(ctx, &array2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&src_array, &array2, sizeof(nested_array_t)) == 0);
}
