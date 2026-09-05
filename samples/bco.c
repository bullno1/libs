#include "../bco.h"
#include <stdlib.h>

// Note: Ignore the `//! [label]` markers, they are for for Doxygen
// They are pushed out of the way so you don't need to pay attention to them

//!                                                                             [bco_decl]
// Fordward declare a coroutine
bco_decl(subcoro, int i);
//!                                                                             [bco_decl]

//!                                                                             [bco]
// Define a coroutine inline
bco(test, int foo, int bar) {
//!                                                                             [bco_vars]
	// Optional local variables
	bco_vars(
		int i;
		void* scratch_buffer;
	)
//!                                                                             [bco_vars]
	// Mark the beginning of a coroutine block
	bco_begin

	bco_var(scratch_buffer) = malloc(1024);

	for (bco_var(i) = bco_arg(foo); bco_var(i) < bco_arg(bar); ++bco_var(i)) {
		// Calling another coroutine function
		bco_call(subcoro, bco_var(i));
		bco_yield(); // Return control to the caller
	}
//!                                                                             [bco_end]
	// Mark the end of a coroutine block
	bco_end

	// Code after bco_end will be executed even on `bco_terminate` as long
	// as the coroutine has started
	free(bco_var(scratch_buffer));
//!                                                                             [bco_end]
}
//!                                                                             [bco]

//!                                                                             [bco_impl]
// Implement a previously declared coroutine functgion
bco_impl(subcoro) {
	bco_begin
	bco_end
}
//!                                                                             [bco_impl]

//!                                                                             [bco_yield_points]
// A coroutine that can survive a hot reload of its code
bco(reloadable, int frames) {
	bco_vars(int i;)
	// Declare the stable names before bco_begin
	bco_yield_points(WAIT_FRAME, WAIT_SUB)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(frames); ++bco_var(i)) {
		// Suspend at a name instead of a line number: prefix the yield with bco_at
		bco_at(WAIT_FRAME) bco_yield();
	}
	// The same for waiting on a subcoroutine
	bco_at(WAIT_SUB) bco_call(subcoro, 0);
	bco_end
}
//!                                                                             [bco_yield_points]

int main(int argc, const char* argv[]) {
//!                                                                             [bco_spawn]
//!                                                                             [bco_align_t]
	// The library does not manage memory
	// The user needs to hand it a properly aligned buffer
	_Alignas(bco_align_t) char bco_buf[1024];
	bco_t* coro = (void*)bco_buf;
//!                                                                             [bco_align_t]
	bco_spawn(coro, test, 2, 3);
//!                                                                             [bco_spawn]

	// The coroutine can be run until completion
	while (bco_status(coro) != BCO_TERMINATED) { bco_resume(coro); }

	// Or terminated early
	bco_terminate(coro);

//!                                                                             [bco_relocate]
	bco_spawn(coro, reloadable, 3);
	bco_resume(coro);
	// Before swapping code: every coroutine must be parked at a named point
	if (bco_reloadable(coro)) {
		bco_reload_begin(coro);
		// ... unload the old code, load the new one, fix up function pointers ...
		bco_reload_end(coro);
	}
	bco_terminate(coro);
//!                                                                             [bco_relocate]

//!                                                                             [bco_mem_size]
	// The coroutine can be heap-allocated
	bco_t* heap_coro = malloc(bco_mem_size(512));
	// Just remember to free it once you are done
	free(heap_coro);
//!                                                                             [bco_mem_size]
}
