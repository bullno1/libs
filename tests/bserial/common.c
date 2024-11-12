#include "common.h"
#include <stdlib.h>

common_fixture_t common_fixture = {
	.ctx_config = {
		.max_depth = 8,
		.max_num_symbols = 64,
		.max_symbol_len = 32,
		.max_record_fields = 32,
	},
};

void
common_fixture_init(void) {
	bserial_out_t* out = bserial_mem_init_out(&common_fixture.mem_out, NULL);
	void* mem = malloc(bserial_ctx_mem_size(common_fixture.ctx_config));
	common_fixture.out_ctx = bserial_make_ctx(
		mem,
		common_fixture.ctx_config,
		NULL, out
	);
	common_fixture.in_ctx = NULL;
}

void
common_fixture_cleanup(void) {
	free(common_fixture.mem_out.mem);
	free(common_fixture.out_ctx);
	free(common_fixture.in_ctx);
}

bserial_ctx_t*
common_fixture_make_in_ctx(void) {
	bserial_in_t* in = bserial_mem_init_in(&common_fixture.mem_in, common_fixture.mem_out.mem, common_fixture.mem_out.len);
	void* mem = malloc(bserial_ctx_mem_size(common_fixture.ctx_config));
	return common_fixture.in_ctx = bserial_make_ctx(
		mem,
		common_fixture.ctx_config,
		in, NULL
	);
}
