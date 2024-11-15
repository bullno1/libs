#include "common.h"
#include "record.h"
#include <assert.h>

suite_t stdio = {
	.name = "stdio",
	.init = common_fixture_init,
	.cleanup = common_fixture_cleanup,
};

TEST(stdio, round_trip) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },

		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};

	{
		FILE* out_file = fopen("stdio.bserial", "wb");
		assert(out_file != NULL);

		bserial_stdio_out_t stdio_out;
		bserial_ctx_t* out = bserial_make_ctx(
			barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(common_fixture.ctx_config)),
			common_fixture.ctx_config,
			NULL,
			bserial_stdio_init_out(&stdio_out, out_file)
		);
		assert(serialize_original(out, &rec) == BSERIAL_OK);
		fclose(out_file);
	}

	{
		FILE* in_file = fopen("stdio.bserial", "rb");
		assert(in_file != NULL);

		bserial_stdio_in_t stdio_in;
		bserial_ctx_t* in = bserial_make_ctx(
			barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(common_fixture.ctx_config)),
			common_fixture.ctx_config,
			bserial_stdio_init_in(&stdio_in, in_file),
			NULL
		);

		original_t rec2 = { 0 };
		assert(serialize_original(in, &rec2) == BSERIAL_OK);

		assert(memcmp(&rec, &rec2, sizeof(rec)) == 0);
		fclose(in_file);
	}
}
