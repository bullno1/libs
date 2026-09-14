#include "common.h"
#include "record.h"

static btest_suite_t record = {
	.name = "bserial/record",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

BTEST(record, round_trip) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },
		.vec2f = { 4.f, -3.5f },

		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec2 = { 0 };
	BTEST_ASSERT(serialize_original(ctx, &rec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Order of keys should not matter
	original_t rec_flipped = { 0 };
	BTEST_ASSERT(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);
	BTEST_ASSERT(rec_flipped.array_len == 3);
	BTEST_ASSERT(rec_flipped.array[0] == 1);
	BTEST_ASSERT(rec_flipped.array[1] == 2);
	BTEST_ASSERT(rec_flipped.array[2] == 3);
}

BTEST(record, missing_fields) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },
		.vec2f = { 4.f, -3.5f },

		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec_with_str = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_str, 0) == BSERIAL_OK);
	BTEST_ASSERT(strcmp(rec_with_str.str, rec.str) == 0);

	original_t rec_with_array = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_array, 1) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_array.array_len == rec.array_len);
	BTEST_ASSERT(memcmp(rec_with_array.array, rec.array, sizeof(rec.array[0]) * rec.array_len) == 0);

	original_t rec_with_num = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_num, 2) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_num.num == rec.num);

	original_t rec_with_vec2 = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_vec2, 3) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_vec2.vec2f.x == rec.vec2f.x);
	BTEST_ASSERT(rec_with_vec2.vec2f.y == rec.vec2f.y);

	original_t rec_with_table = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_table, 4) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_table.table_len == rec.table_len);
	BTEST_ASSERT(memcmp(rec_with_table.table, rec.table, sizeof(rec.table[0]) * rec.table_len) == 0);
}

// A record whose first member is another record shares its address with that
// member. Nesting must still be detected through the key/value state, not by
// address.
typedef struct {
	vec2f_t pos;
	int id;
} nested_first_t;

typedef struct {
	nested_first_t inner;
	float scale;
} nested_twice_t;

static bserial_status_t
serialize_nested_first(bserial_ctx_t* ctx, nested_first_t* rec) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, pos) {
			BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->pos));
		}

		BSERIAL_KEY(ctx, id) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->id));
		}
	}

	return bserial_status(ctx);
}

static bserial_status_t
serialize_nested_twice(bserial_ctx_t* ctx, nested_twice_t* rec) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, inner) {
			BSERIAL_CHECK_STATUS(serialize_nested_first(ctx, &rec->inner));
		}

		BSERIAL_KEY(ctx, scale) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->scale));
		}
	}

	return bserial_status(ctx);
}

BTEST(record, nested_first_member) {
	nested_twice_t rec = {
		.inner = {
			.pos = { 1.5f, -2.5f },
			.id = 7,
		},
		.scale = 0.25f,
	};
	BTEST_ASSERT((void*)&rec == (void*)&rec.inner);
	BTEST_ASSERT((void*)&rec == (void*)&rec.inner.pos);

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_nested_twice(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_nested_twice(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	nested_twice_t rec2 = { 0 };
	BTEST_ASSERT(serialize_nested_twice(ctx, &rec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	nested_twice_t rec3 = { 0 };
	BTEST_ASSERT(serialize_nested_twice(ctx, &rec3) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec3, sizeof(rec)) == 0);
}
