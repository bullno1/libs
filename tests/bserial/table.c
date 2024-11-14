#include "common.h"
#include "record.h"
#include <assert.h>

static suite_t table = {
	.name = "table",
	.init = common_fixture_init,
	.cleanup = common_fixture_cleanup,
};

TEST(table, round_trip) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 }
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;

	uint64_t len = 2;
	assert(bserial_table(ctx, &len) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec_flipped = { 0 };
	uint64_t read_len;
	assert(bserial_table(ctx, &read_len) == BSERIAL_OK);
	assert(len == read_len);

	original_t rec2 = { 0 };
	assert(serialize_original(ctx, &rec2) == BSERIAL_OK);
	assert(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Order of keys should not matter
	assert(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	assert(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);
	assert(rec_flipped.array_len == 3);
	assert(rec_flipped.array[0] == 1);
	assert(rec_flipped.array[1] == 2);
	assert(rec_flipped.array[2] == 3);
}
