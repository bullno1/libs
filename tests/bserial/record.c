#include "common.h"
#include "record.h"
#include <assert.h>

static suite_t record = {
	.name = "record",
	.init = common_fixture_init,
	.cleanup = common_fixture_cleanup,
};

TEST(record, round_trip) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 }
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec2 = { 0 };
	assert(serialize_original(ctx, &rec2) == BSERIAL_OK);
	assert(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Order of keys should not matter
	original_t rec_flipped = { 0 };
	assert(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	assert(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);
	assert(rec_flipped.array_len == 3);
	assert(rec_flipped.array[0] == 1);
	assert(rec_flipped.array[1] == 2);
	assert(rec_flipped.array[2] == 3);
}

TEST(record, missing_fields) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },
		.vec2f = { 4.f, -3.5f },
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec_with_str = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_str, 0) == BSERIAL_OK);
	assert(strcmp(rec_with_str.str, rec.str) == 0);

	original_t rec_with_array = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_array, 1) == BSERIAL_OK);
	assert(rec_with_array.array_len == rec.array_len);
	assert(memcmp(rec_with_array.array, rec.array, sizeof(rec.array[0]) * rec.array_len) == 0);

	original_t rec_with_num = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_num, 2) == BSERIAL_OK);
	assert(rec_with_num.num == rec.num);

	original_t rec_with_vec2 = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_vec2, 3) == BSERIAL_OK);
	assert(rec_with_vec2.vec2f.x == rec.vec2f.x);
	assert(rec_with_vec2.vec2f.y == rec.vec2f.y);
}
