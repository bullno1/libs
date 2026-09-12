#include "common.h"
#include "record.h"
#include <string.h>

static btest_suite_t enums = {
	.name = "bserial/enum",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

enum {
	SHAPE_CIRCLE = 10,
	SHAPE_SQUARE = 20,
	SHAPE_TRIANGLE = 30,
};

static bserial_status_t
serialize_shape(bserial_ctx_t* ctx, int* shape) {
	BSERIAL_ENUM(ctx, shape) {
		BSERIAL_VARIANT(ctx, SHAPE_CIRCLE);
		BSERIAL_VARIANT(ctx, SHAPE_SQUARE);
		BSERIAL_VARIANT(ctx, SHAPE_TRIANGLE);
	}

	return bserial_status(ctx);
}

// Same names, different numeric values: only names hit the stream
enum {
	SHAPE2_CIRCLE = 1,
	SHAPE2_SQUARE = 2,
	SHAPE2_TRIANGLE = 3,
};

static bserial_status_t
serialize_shape_renumbered(bserial_ctx_t* ctx, int* shape) {
	BSERIAL_ENUM(ctx, shape) {
		bserial_variant(ctx, "SHAPE_CIRCLE", sizeof("SHAPE_CIRCLE") - 1, SHAPE2_CIRCLE);
		bserial_variant(ctx, "SHAPE_SQUARE", sizeof("SHAPE_SQUARE") - 1, SHAPE2_SQUARE);
		bserial_variant(ctx, "SHAPE_TRIANGLE", sizeof("SHAPE_TRIANGLE") - 1, SHAPE2_TRIANGLE);
	}

	return bserial_status(ctx);
}

// SHAPE_SQUARE was renamed to SHAPE_BOX; old data is still readable
enum {
	SHAPE3_CIRCLE = 10,
	SHAPE3_BOX = 20,
};

static bserial_status_t
serialize_shape_renamed(bserial_ctx_t* ctx, int* shape) {
	BSERIAL_ENUM(ctx, shape) {
		if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
			bserial_variant(ctx, "SHAPE_SQUARE", sizeof("SHAPE_SQUARE") - 1, SHAPE3_BOX);
		}
		BSERIAL_VARIANT(ctx, SHAPE3_CIRCLE);
		BSERIAL_VARIANT(ctx, SHAPE3_BOX);
	}

	return bserial_status(ctx);
}

BTEST(enums, round_trip) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int a = SHAPE_SQUARE;
	int b = SHAPE_TRIANGLE;
	int c = SHAPE_SQUARE;
	BTEST_ASSERT(serialize_shape(ctx, &a) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape(ctx, &b) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape(ctx, &c) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	// Two definitions and one reference
	size_t expected_len =
		1 + 1 + (sizeof("SHAPE_SQUARE") - 1)
		+ 1 + 1 + (sizeof("SHAPE_TRIANGLE") - 1)
		+ 1 + 1;
	BTEST_ASSERT(common_fixture.mem_out.len == expected_len);

	ctx = common_fixture_make_in_ctx();
	int a2 = 0, b2 = 0, c2 = 0;
	BTEST_ASSERT(serialize_shape(ctx, &a2) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape(ctx, &b2) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape(ctx, &c2) == BSERIAL_OK);
	BTEST_ASSERT(a2 == SHAPE_SQUARE);
	BTEST_ASSERT(b2 == SHAPE_TRIANGLE);
	BTEST_ASSERT(c2 == SHAPE_SQUARE);

	// Numeric values can change as long as names are stable
	ctx = common_fixture_make_in_ctx();
	BTEST_ASSERT(serialize_shape_renumbered(ctx, &a2) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape_renumbered(ctx, &b2) == BSERIAL_OK);
	BTEST_ASSERT(serialize_shape_renumbered(ctx, &c2) == BSERIAL_OK);
	BTEST_ASSERT(a2 == SHAPE2_SQUARE);
	BTEST_ASSERT(b2 == SHAPE2_TRIANGLE);
	BTEST_ASSERT(c2 == SHAPE2_SQUARE);
}

BTEST(enums, rename) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int old = SHAPE_SQUARE;
	BTEST_ASSERT(serialize_shape(ctx, &old) == BSERIAL_OK);

	ctx = common_fixture_make_in_ctx();
	int renamed = 0;
	BTEST_ASSERT(serialize_shape_renamed(ctx, &renamed) == BSERIAL_OK);
	BTEST_ASSERT(renamed == SHAPE3_BOX);
}

BTEST(enums, unknown_variant) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int triangle = SHAPE_TRIANGLE;
	BTEST_ASSERT(serialize_shape(ctx, &triangle) == BSERIAL_OK);

	// The renamed enum has no triangle
	ctx = common_fixture_make_in_ctx();
	int value = -1;
	BTEST_ASSERT(serialize_shape_renamed(ctx, &value) == BSERIAL_MALFORMED);
	BTEST_ASSERT(value == -1);
}

BTEST(enums, unnamed_value) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int bogus = 12345;
	BTEST_ASSERT(serialize_shape(ctx, &bogus) == BSERIAL_MALFORMED);
}

typedef struct {
	int shape;
	vec2f_t pos;
	int history_len;
	int history[4];
} entity_t;

static bserial_status_t
serialize_entity(bserial_ctx_t* ctx, entity_t* entity) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, shape) {
			BSERIAL_CHECK_STATUS(serialize_shape(ctx, &entity->shape));
		}

		BSERIAL_KEY(ctx, pos) {
			BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &entity->pos));
		}

		BSERIAL_KEY(ctx, history) {
			uint64_t len = (uint64_t)entity->history_len;
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
			if (len > 4) { return BSERIAL_MALFORMED; }
			entity->history_len = (int)len;
			for (int i = 0; i < entity->history_len; ++i) {
				BSERIAL_CHECK_STATUS(serialize_shape(ctx, &entity->history[i]));
			}
		}
	}

	return bserial_status(ctx);
}

// Does not know about shape or history: both must be skipped
static bserial_status_t
serialize_entity_pos_only(bserial_ctx_t* ctx, entity_t* entity) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, pos) {
			BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &entity->pos));
		}
	}

	return bserial_status(ctx);
}

BTEST(enums, nested) {
	entity_t entity = {
		.shape = SHAPE_CIRCLE,
		.pos = { 1.f, 2.f },
		.history_len = 3,
		.history = { SHAPE_SQUARE, SHAPE_CIRCLE, SHAPE_TRIANGLE },
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_entity(ctx, &entity) == BSERIAL_OK);
	BTEST_ASSERT(serialize_entity(ctx, &entity) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	entity_t entity2 = { 0 };
	BTEST_ASSERT(serialize_entity(ctx, &entity2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&entity, &entity2, sizeof(entity)) == 0);

	entity_t entity3 = { 0 };
	BTEST_ASSERT(serialize_entity_pos_only(ctx, &entity3) == BSERIAL_OK);
	BTEST_ASSERT(entity3.pos.x == 1.f);
	BTEST_ASSERT(entity3.pos.y == 2.f);
	BTEST_ASSERT(entity3.shape == 0);
	BTEST_ASSERT(entity3.history_len == 0);
}

BTEST(enums, variant_outside_enum) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(BSERIAL_VARIANT(ctx, SHAPE_CIRCLE) == BSERIAL_MALFORMED);
}
