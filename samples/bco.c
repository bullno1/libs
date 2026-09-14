#include "../bco.h"
#include <stdio.h>
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

//!                                                                             [bco_recv]
typedef struct { int damage; } hit_t;

// A coroutine that waits for values handed to it from the outside
bco(listener, int hp) {
	// A received value lands in a coroutine variable so it survives later suspensions
	bco_vars(hit_t hit;)
	bco_yield_points(WAIT_HIT)
	bco_begin
	while (bco_arg(hp) > 0) {
		// Suspend until a hit_t is handed over with bco_send
		bco_at(WAIT_HIT) bco_recv(hit_t, hit);
		bco_arg(hp) -= bco_var(hit).damage;
	}
	bco_end
}
//!                                                                             [bco_recv]

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

//!                                                                             [bco_reloadable]
	bco_spawn(coro, reloadable, 3);
	bco_resume(coro);
	// Before swapping code: every coroutine must be parked at a named point
	bco_loc_t blocker;
	if (bco_reloadable(coro, &blocker)) {
		bco_reload_begin(coro);
		// ... unload the old code, load the new one, fix up function pointers ...
		bco_reload_end(coro);
	} else {
		// Points at the bco_yield, bco_join or bco_call that needs a bco_at prefix
		fprintf(stderr, "%s:%d: plain yield blocks reload\n", blocker.file, blocker.line);
	}
	bco_terminate(coro);
//!                                                                             [bco_reloadable]

//!                                                                             [bco_send]
	bco_spawn(coro, listener, 10);
	// Run it up to its bco_recv: a coroutine that has not started is not waiting
	bco_resume(coro);
	// Accepted only while the coroutine is parked at a matching bco_recv
	if (bco_send(coro, hit_t, { .damage = 4 })) {
		bco_resume(coro);  // Picks the value up and runs to the next bco_recv
	}
	// An existing variable works too
	hit_t hit = { .damage = 6 };
	bco_send(coro, hit_t, hit);
	bco_resume(coro);
	bco_terminate(coro);
//!                                                                             [bco_send]

//!                                                                             [bco_mem_size]
	// The coroutine can be heap-allocated
	bco_t* heap_coro = malloc(bco_mem_size(512));
	// Just remember to free it once you are done
	free(heap_coro);
//!                                                                             [bco_mem_size]
}
