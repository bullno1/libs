#ifndef BFMT_TEST_SHARED_H
#define BFMT_TEST_SHARED_H

#include <string.h>
#include "../../bfmt.h"
#include "../../btest.h"

// Everything printed to OUT ends up here
static struct {
	bfmt_stream_t stream;
	char buf[4096];
	int len;
	int num_writes;
} fixture;

static inline void
fixture_write(void* userdata, const char* str, int len) {
	(void)userdata;
	int space = (int)sizeof(fixture.buf) - 1 - fixture.len;
	if (len > space) { len = space; }
	memcpy(fixture.buf + fixture.len, str, (size_t)len);
	fixture.len += len;
	fixture.buf[fixture.len] = '\0';
	fixture.num_writes += 1;
}

static inline void
reset_output(void) {
	fixture.len = 0;
	fixture.buf[0] = '\0';
	fixture.num_writes = 0;
}

static inline void
init_output(void) {
	fixture.stream = (bfmt_stream_t){ .write = fixture_write };
	reset_output();
}

#define OUT (&fixture.stream)

// Compare everything printed since the last check, then start over
#define EXPECT_OUTPUT(EXPECTED) \
	do { \
		BTEST_EXPECT_EX(strcmp(fixture.buf, EXPECTED) == 0, "got \"%s\"", fixture.buf); \
		reset_output(); \
	} while (0)

#endif
