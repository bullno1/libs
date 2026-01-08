#include "shared.h"
#include "../../btest.h"

static btest_suite_t versioning = {
	.name = "bsv/versioning",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

typedef struct {
	int a;
} v1_t;

bsv_status_t
v1(bsv_ctx_t* ctx, v1_t* v) {
	BSV_BLK(ctx, 0) {
		BSV_REV(0) {
			BSV_ADD(&v->a);
		}
	}

	return bsv_status(ctx);
}

typedef struct {
	int a;
	int b;
} v2_t;

bsv_status_t
v2(bsv_ctx_t* ctx, v2_t* v) {
	BSV_BLK(ctx, 1) {
		BSV_REV(0) {
			BSV_ADD(&v->a);
		}

		BSV_REV(1) {
			BSV_ADD(&v->b);
		}
	}

	return bsv_status(ctx);
}

typedef struct {
	int a;
	float b;
} v3_t;

bsv_status_t
v3(bsv_ctx_t* ctx, v3_t* v) {
	BSV_BLK(ctx, 2) {
		BSV_REV(0) {
			BSV_ADD(&v->a);
		}

		BSV_REV(1) {
			int ib = 0;
			BSV_REM(&ib, 2) {
				v->b = (float)ib;
			}
		}

		BSV_REV(2) {
			BSV_ADD(&v->b);
		}
	}

	return bsv_status(ctx);
}

BTEST(versioning, v1_to_v2) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	v1_t v1_obj = {
		.a = 67,
	};
	BTEST_EXPECT_EQUAL("%d", v1(&out, &v1_obj), BSV_OK);

	bsv_ctx_t in = { .in = bsv_mem_in() };
	v2_t v2_obj = { 0 };
	BTEST_EXPECT_EQUAL("%d", v2(&in, &v2_obj), BSV_OK);

	BTEST_EXPECT_EQUAL("%d", v2_obj.a, v1_obj.a);
	BTEST_EXPECT_EQUAL("%d", v2_obj.b, 0);
}

BTEST(versioning, v2_to_v3) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	v2_t v2_obj = {
		.a = 67,
		.b = -256,
	};
	BTEST_EXPECT_EQUAL("%d", v2(&out, &v2_obj), BSV_OK);

	bsv_ctx_t in = { .in = bsv_mem_in() };
	v3_t v3_obj = { 0 };
	BTEST_EXPECT_EQUAL("%d", v3(&in, &v3_obj), BSV_OK);

	BTEST_EXPECT_EQUAL("%d", v3_obj.a, v2_obj.a);
	BTEST_EXPECT_EQUAL("%f", v3_obj.b, -256.0f);
}

BTEST(versioning, v1_to_v3) {
	bsv_ctx_t out = { .out = bsv_mem_out() };

	v1_t v1_obj = {
		.a = 67,
	};
	BTEST_EXPECT_EQUAL("%d", v1(&out, &v1_obj), BSV_OK);

	bsv_ctx_t in = { .in = bsv_mem_in() };
	v3_t v3_obj = { 0 };
	BTEST_EXPECT_EQUAL("%d", v3(&in, &v3_obj), BSV_OK);

	BTEST_EXPECT_EQUAL("%d", v3_obj.a, v1_obj.a);
	BTEST_EXPECT_EQUAL("%f", v3_obj.b, 0.0f);
}
