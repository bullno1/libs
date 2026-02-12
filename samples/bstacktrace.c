#include "../bstacktrace.h"
#include <stdio.h>
#include <inttypes.h>

static bstacktrace_t* bstacktrace = NULL;

static bool
stack_walk(uintptr_t address, void* userdata) {
	bstacktrace_info_t info = bstacktrace_resolve(bstacktrace, address, BSTACKTRACE_RESOLVE_ALL);
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
	bstacktrace_walk(bstacktrace, stack_walk, NULL);
}

static void
foo(void) {
	bar();
}

int
main(int argc, const char* argv[]) {
	bstacktrace = bstacktrace_init(NULL);
	foo();

	bstacktrace_cleanup(bstacktrace);

	return 0;
}

#define BLIB_IMPLEMENTATION
#include "../bstacktrace.h"
