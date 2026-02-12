#include "../../bstacktrace.h"
#include "../../btest.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

static struct {
	bstacktrace_t* ctx;
} fixture = { 0 };

static void
init_per_test(void) {
	fixture.ctx = bstacktrace_init(NULL);
}

static void
cleanup_per_test(void) {
	bstacktrace_cleanup(fixture.ctx);
}

static btest_suite_t bstacktrace_ = {
	.name = "bstacktrace",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static bool
walk_once(uintptr_t address, void* userdata) {
	bstacktrace_info_t info = bstacktrace_resolve(fixture.ctx, address, BSTACKTRACE_RESOLVE_ALL);
	BTEST_EXPECT(info.function != NULL && strcmp(info.function, "bar") == 0);

	return false;
}

static bool
walk_all(uintptr_t address, void* userdata) {
	bstacktrace_info_t info = bstacktrace_resolve(fixture.ctx, address, BSTACKTRACE_RESOLVE_ALL);
	printf(
		"%s (%p) in %s at %s:%d:%d\n",
		info.function, (void*)address,
		info.module,
		info.filename, info.line, info.column
	);

	return true;
}

static void
bar(void) {
	bstacktrace_walk(fixture.ctx, walk_once, NULL);
	bstacktrace_walk(fixture.ctx, walk_all, NULL);
}

static void
foo(void) {
	bar();
}

BTEST(bstacktrace_, basic) {
	foo();
}

#define BLIB_IMPLEMENTATION
#include "../../bstacktrace.h"
#include "../../barena.h"
