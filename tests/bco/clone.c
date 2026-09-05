#include "shared.h"
#include "../../btest.h"

#include <stdint.h>



/**
 * Cloning is meant for retargeting a coroutine into another environment, so
 * each instance is given its own environment through the userdata.
 *
 * Carrying the storage bounds lets a body assert that its frame really lives in
 * its own buffer. Without that check a clone that still points into the
 * source's memory reads all the right values and looks correct.
 */
typedef struct {
	const char* name;
	const char* base;
	size_t size;
} clone_env_t;

static clone_env_t env_src;
static clone_env_t env_dst;

static void
clone_init_per_test(void) {
	init_per_test();
	env_src = (clone_env_t){ "src", fixture.mem_a, sizeof(fixture.mem_a) };
	env_dst = (clone_env_t){ "dst", fixture.mem_b, sizeof(fixture.mem_b) };
}

static btest_suite_t clone = {
	.name = "bco/clone",
	.init_per_test = clone_init_per_test,
};

/*
 * `counter` and `overaligned` below are deliberately named the same as the
 * coroutines in basic.c. Both files use bco_static, so the names stay private
 * and the link succeeds. Do not rename them to be "unique".
 */

/// Whether `p` points inside the storage this coroutine was given
static int
owns(const clone_env_t* env, const void* p) {
	uintptr_t addr = (uintptr_t)p;
	uintptr_t base = (uintptr_t)env->base;
	return addr >= base && addr < base + env->size;
}

bco_static(counter, int to) {
	bco_vars(int i;)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(to); ++bco_var(i)) {
		{  // Scoped so that it does not live across the yield below
			const clone_env_t* env = bco_userdata;
			trace("%s:%d:own=%d", env->name, bco_var(i), owns(env, &bco_var(i)));
		}
		bco_yield();
	}
	bco_end
	trace("%s:done", ((const clone_env_t*)bco_userdata)->name);
}

BTEST(clone, copy_runs_independently_of_the_source) {
	bco_spawn(coro_a(), counter, 3);
	bco_set_userdata(coro_a(), &env_src);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("src:0:own=1");

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	// The clone picks up exactly where the source was left, from its own memory
	drive(coro_b());
	BCO_EXPECT_TRACE("src:0:own=1 dst:1:own=1 dst:2:own=1 dst:done");

	// ...and the source is untouched by the clone having run
	drive(coro_a());
	BCO_EXPECT_TRACE(
		"src:0:own=1 dst:1:own=1 dst:2:own=1 dst:done "
		"src:1:own=1 src:2:own=1 src:done"
	);

	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_TERMINATED);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_b()), BCO_TERMINATED);
}

// Interleaving the two instances is what actually proves the frames are
// separate: a shared counter would make one of them skip steps.
BTEST(clone, source_and_copy_interleave_without_sharing_state) {
	bco_spawn(coro_a(), counter, 3);
	bco_set_userdata(coro_a(), &env_src);
	bco_resume(coro_a());

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	bco_resume(coro_b());
	bco_resume(coro_a());
	bco_resume(coro_b());
	bco_resume(coro_a());

	BCO_EXPECT_TRACE(
		"src:0:own=1 dst:1:own=1 src:1:own=1 dst:2:own=1 src:2:own=1"
	);
}

BTEST(clone, copy_of_a_never_resumed_coroutine) {
	bco_spawn(coro_a(), counter, 2);
	bco_set_userdata(coro_a(), &env_src);

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	drive(coro_b());
	BCO_EXPECT_TRACE("dst:0:own=1 dst:1:own=1 dst:done");

	drive(coro_a());
	BCO_EXPECT_TRACE(
		"dst:0:own=1 dst:1:own=1 dst:done src:0:own=1 src:1:own=1 src:done"
	);
}

bco_static(clone_sub, int rounds) {
	bco_vars(int i;)
	bco_begin
	for (bco_var(i) = 0; bco_var(i) < bco_arg(rounds); ++bco_var(i)) {
		{  // Scoped so that it does not live across the yield below
			const clone_env_t* env = bco_userdata;
			trace("%s:sub:%d:own=%d", env->name, bco_var(i), owns(env, &bco_var(i)));
		}
		bco_yield();
	}
	bco_end
	trace("%s:sub:cleanup", ((const clone_env_t*)bco_userdata)->name);
}

bco_static(clone_outer, int rounds) {
	bco_vars(int marker;)
	bco_begin
	bco_var(marker) = 0xf00;
	bco_call(clone_sub, bco_arg(rounds));
	trace("%s:outer:marker=%#x", ((const clone_env_t*)bco_userdata)->name, bco_var(marker));
	bco_end
}

/**
 * A subcoroutine's frame lives past the parent's stack pointer, so a copy that
 * only walks the parent's frame leaves the clone's subcoroutine pointing back
 * into the source's memory. It reads correct values that way, which is why this
 * test interleaves the two instances and checks frame ownership rather than
 * just comparing values.
 */
BTEST(clone, copy_taken_while_inside_a_subcall) {
	bco_spawn(coro_a(), clone_outer, 3);
	bco_set_userdata(coro_a(), &env_src);

	bco_resume(coro_a());
	BCO_EXPECT_TRACE("src:sub:0:own=1");

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	bco_resume(coro_b());
	bco_resume(coro_a());

	BCO_EXPECT_TRACE("src:sub:0:own=1 dst:sub:1:own=1 src:sub:1:own=1");

	drive(coro_b());
	drive(coro_a());

	BCO_EXPECT_TRACE(
		"src:sub:0:own=1 dst:sub:1:own=1 src:sub:1:own=1 "
		"dst:sub:2:own=1 dst:sub:cleanup dst:outer:marker=0xf00 "
		"src:sub:2:own=1 src:sub:cleanup src:outer:marker=0xf00"
	);
}

bco_static(overaligned, int unused) {
	bco_vars(_Alignas(16) char blob[16]; int tag;)
	bco_begin
	bco_var(tag) = 0x2222;
	bco_var(blob)[0] = 9;
	bco_yield();
	trace("%s:aligned=%d own=%d tag=%#x blob0=%d",
		((const clone_env_t*)bco_userdata)->name,
		((uintptr_t)bco_var(blob) % 16u) == 0,
		owns(bco_userdata, bco_var(blob)),
		bco_var(tag),
		(int)bco_var(blob)[0]);
	bco_end
}

// Offsets inside the frame must survive relocation into a different buffer,
// including for vars needing more alignment than a pointer.
BTEST(clone, overaligned_vars_survive_relocation) {
	bco_spawn(coro_a(), overaligned, 0);
	bco_set_userdata(coro_a(), &env_src);
	bco_resume(coro_a());

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	drive(coro_b());
	BCO_EXPECT_TRACE("dst:aligned=1 own=1 tag=0x2222 blob0=9");

	drive(coro_a());
	BCO_EXPECT_TRACE(
		"dst:aligned=1 own=1 tag=0x2222 blob0=9 src:aligned=1 own=1 tag=0x2222 blob0=9"
	);
}

// Terminating one instance must not disturb the other.
BTEST(clone, source_and_copy_terminate_separately) {
	bco_spawn(coro_a(), counter, 5);
	bco_set_userdata(coro_a(), &env_src);
	bco_resume(coro_a());

	bco_copy(coro_b(), coro_a());
	bco_set_userdata(coro_b(), &env_dst);

	bco_terminate(coro_b());
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_b()), BCO_TERMINATED);
	BTEST_EXPECT_EQUAL("%d", bco_status(coro_a()), BCO_SUSPENDED);
	BCO_EXPECT_TRACE("src:0:own=1 dst:done");

	drive(coro_a());
	BCO_EXPECT_TRACE(
		"src:0:own=1 dst:done src:1:own=1 src:2:own=1 src:3:own=1 src:4:own=1 src:done"
	);
}
