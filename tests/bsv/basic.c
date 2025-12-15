#include "shared.h"
#include "../../btest.h"

static btest_suite_t basic = {
	.name = "bsv/basic",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BTEST(basic, round_trip) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	int a = -3;
	unsigned int b = 4;
	float f = 6.7f;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &b), BSV_OK);
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &f), BSV_OK);

	bsv_ctx_t in = { .in = bsv_mem_in() };
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&in, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&in, &b), BSV_OK);
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&in, &f), BSV_OK);

	BTEST_EXPECT_EQUAL("%d", a, -3);
	BTEST_EXPECT_EQUAL("%d", b, 4);
	BTEST_EXPECT_EQUAL("%f", f, 6.7f);
}

BTEST(basic, varint_unsigned) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	unsigned int a = 3;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 1);

	out.out = bsv_mem_out();
	a = 400;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 2);
}

BTEST(basic, varint_signed) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	int a = 3;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 1);

	out.out = bsv_mem_out();
	a = 400;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 2);

	out.out = bsv_mem_out();
	a = -3;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 1);

	out.out = bsv_mem_out();
	a = -400;
	BTEST_EXPECT_EQUAL("%d", bsv_auto(&out, &a), BSV_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem_out.len, 2);
}

BTEST(basic, array) {
	int array[] = { 1, 2, 3, 4 };

	bsv_ctx_t out = { .out = bsv_mem_out() };
	BSV_WITH(&out) {
		bsv_len_t len = sizeof(array) / sizeof(array[0]);
		BSV_ARRAY(&len);
		for (bsv_len_t i = 0; i < len; ++i) {
			bsv_auto(BSV_CTX, &array[i]);
		}
	}

	int array_in[4] = { 0 };
	bsv_ctx_t in = { .in = bsv_mem_in() };
	BSV_WITH(&in) {
		bsv_len_t len = 0;
		BSV_ARRAY(&len);
		for (bsv_len_t i = 0; i < len; ++i) {
			bsv_auto(BSV_CTX, &array_in[i]);
		}
	}

	for (bsv_len_t i = 0; i < sizeof(array) / sizeof(array[0]); ++i) {
		BTEST_EXPECT_EQUAL("%d", array[i], array_in[i]);
	}
}
