#include "common.h"
#include <stdlib.h>
#include <stdio.h>

common_fixture_t common_fixture = {
	.ctx_config = {
		.max_depth = 8,
		.max_num_symbols = 64,
		.max_symbol_len = 32,
		.max_record_fields = 32,
	},
};

barena_pool_t pool = { 0 };

void
common_fixture_init(void) {
	if (pool.chunk_size == 0) {
		barena_pool_init(&pool, 4096);
	}
	barena_init(&common_fixture.arena, &pool);

	bserial_out_t* out = bserial_mem_init_out(&common_fixture.mem_out, NULL);
	void* mem = barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(common_fixture.ctx_config));
	common_fixture.out_ctx = bserial_make_ctx(
		mem,
		common_fixture.ctx_config,
		NULL, out
	);
	common_fixture.in_ctx = NULL;
}

void
common_fixture_cleanup(void) {
	barena_reset(&common_fixture.arena);
	free(common_fixture.mem_out.mem);
}

bserial_ctx_t*
common_fixture_make_in_ctx(void) {
	bserial_in_t* in = bserial_mem_init_in(&common_fixture.mem_in, common_fixture.mem_out.mem, common_fixture.mem_out.len);
	void* mem = barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(common_fixture.ctx_config));
	return common_fixture.in_ctx = bserial_make_ctx(
		mem,
		common_fixture.ctx_config,
		in, NULL
	);
}

// https://stackoverflow.com/a/73630740
void
hex_dump(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}
