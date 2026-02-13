#include "../bstacktrace.h"
#include "../bcrash_handler.h"
#include <stdio.h>
#include <inttypes.h>

static bstacktrace_t* bstacktrace = NULL;

//!                                                                             [bcrash_handler_fn_t]
static bool
stack_walk(uintptr_t address, void* userdata) {
	int* num_skips = userdata;
	if (*num_skips > 0) {
		*num_skips -= 1;
		return true;
	}

	bstacktrace_info_t info = bstacktrace_resolve(bstacktrace, address, BSTACKTRACE_RESOLVE_ALL);
	fprintf(
		stderr,
		"%s (%p) in %s at %s:%d:%d\n",
		info.function, (void*)address,
		info.module,
		info.filename, info.line, info.column
	);

	return true;
}

static void
crash_handler(bcrash_info_t crash_info) {
	fprintf(stderr, "Error %d at address %p, pc = %p\n", crash_info.code, (void*)crash_info.fault_addr, (void*)crash_info.pc);

	bstacktrace_info_t info = bstacktrace_resolve(bstacktrace, crash_info.pc, BSTACKTRACE_RESOLVE_ALL | BSTACKTRACE_RESOLVE_DIRECT);
	fprintf(
		stderr,
		"%s (%p) in %s at %s:%d:%d\n",
		info.function, (void*)crash_info.pc,
		info.module,
		info.filename, info.line, info.column
	);

	// Skip the signal handler and the caller
	int num_skips = crash_info.num_handler_frames;
	bstacktrace_walk(bstacktrace, stack_walk, &num_skips);
}
//!                                                                             [bcrash_handler_fn_t]

static void
bar(void) {
	volatile int* a = (int*)0x01;
	*a = 420;
}

static void
foo(void) {
	bar();
}

int
main(int argc, const char* argv[]) {
	bstacktrace = bstacktrace_init(NULL);
	bcrash_handler_set(crash_handler);

	foo();

	bstacktrace_cleanup(bstacktrace);
	return 0;
}

#define BLIB_IMPLEMENTATION
#include "../bstacktrace.h"
#include "../bcrash_handler.h"
