#ifndef BSERIAL_TEST_COMMON_H
#define BSERIAL_TEST_COMMON_H

#define BSERIAL_MEM
#define BSERIAL_STDIO
#include "../../bserial.h"
#include "../../barena.h"
#include "../../btest.h"

typedef struct {
	bserial_mem_out_t mem_out;
	bserial_mem_in_t mem_in;
	bserial_ctx_config_t ctx_config;
	bserial_ctx_t* out_ctx;
	bserial_ctx_t* in_ctx;
	barena_t arena;
} common_fixture_t;

extern common_fixture_t common_fixture;

void
common_fixture_init(void);

void
common_fixture_cleanup(void);

bserial_ctx_t*
common_fixture_make_in_ctx(void);

void
hex_dump(const void* data, size_t size);

void
trace_bserial_ctx(int depth, const char* fmt, va_list args, void* userdata);

#endif
