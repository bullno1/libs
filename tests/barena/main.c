#include "../../barena.h"
#include "../../btest.h"
#include <string.h>
#include <stdint.h>

#define CHUNK_SIZE (4096 * 2)

static struct {
	barena_pool_t pool;
	barena_t arena;
} fixture;

static void
init_per_test(void) {
	barena_pool_init(&fixture.pool, CHUNK_SIZE);
	barena_init(&fixture.arena, &fixture.pool);
}

static void
cleanup_per_test(void) {
	// Chunks still held by the arena are only released once returned to the
	// pool
	barena_reset(&fixture.arena);
	barena_pool_cleanup(&fixture.pool);
}

static btest_suite_t barena_ = {
	.name = "barena",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static bool
is_aligned(const void* ptr, size_t alignment) {
	return ((uintptr_t)ptr % alignment) == 0;
}

BTEST(barena_, alignment) {
	barena_t* arena = &fixture.arena;

	// Default alignment is suitable for any type
	void* a = barena_malloc(arena, 1);
	BTEST_ASSERT(a != NULL);
	BTEST_EXPECT(is_aligned(a, _Alignof(double)));
	void* b = barena_malloc(arena, 1);
	BTEST_ASSERT(b != NULL);
	BTEST_EXPECT(is_aligned(b, _Alignof(double)));
	BTEST_EXPECT(a != b);

	// Explicit alignment
	void* c = barena_memalign(arena, 1, 64);
	BTEST_ASSERT(c != NULL);
	BTEST_EXPECT(is_aligned(c, 64));
	void* d = barena_memalign(arena, 100, 1024);
	BTEST_ASSERT(d != NULL);
	BTEST_EXPECT(is_aligned(d, 1024));
}

BTEST(barena_, zero_size) {
	BTEST_EXPECT(barena_malloc(&fixture.arena, 0) == NULL);
}

BTEST(barena_, chunk_growth) {
	barena_t* arena = &fixture.arena;
	enum { NUM_BLOCKS = 64, BLOCK_SIZE = 1000 };  // Way more than a chunk
	unsigned char* blocks[NUM_BLOCKS];

	for (int i = 0; i < NUM_BLOCKS; ++i) {
		blocks[i] = barena_malloc(arena, BLOCK_SIZE);
		BTEST_ASSERT(blocks[i] != NULL);
		memset(blocks[i], i, BLOCK_SIZE);
	}

	// No block overlaps another
	for (int i = 0; i < NUM_BLOCKS; ++i) {
		for (int j = 0; j < BLOCK_SIZE; ++j) {
			BTEST_ASSERT_EQUAL("%d", blocks[i][j], i);
		}
	}
}

BTEST(barena_, larger_than_chunk) {
	barena_t* arena = &fixture.arena;

	void* small = barena_malloc(arena, 16);
	BTEST_ASSERT(small != NULL);

	// A request that does not fit in a chunk gets a dedicated one
	size_t big_size = CHUNK_SIZE * 3;
	unsigned char* big = barena_malloc(arena, big_size);
	BTEST_ASSERT(big != NULL);
	memset(big, 0xab, big_size);
	BTEST_EXPECT_EQUAL("%d", big[0], 0xab);
	BTEST_EXPECT_EQUAL("%d", big[big_size - 1], 0xab);

	// The arena is still usable afterwards
	void* after = barena_malloc(arena, 16);
	BTEST_ASSERT(after != NULL);
	BTEST_EXPECT(after != small);
}

BTEST(barena_, snapshot_restore) {
	barena_t* arena = &fixture.arena;

	// An empty arena has a null snapshot which restore accepts
	barena_snapshot_t empty = barena_snapshot(arena);
	BTEST_EXPECT(empty == NULL);
	barena_restore(arena, empty);
	BTEST_EXPECT(barena_snapshot(arena) == NULL);

	void* before = barena_malloc(arena, 100);
	BTEST_ASSERT(before != NULL);
	barena_snapshot_t snapshot = barena_snapshot(arena);
	BTEST_ASSERT(snapshot != NULL);

	// Allocate across several chunks after the snapshot
	void* first_after = barena_malloc(arena, 100);
	for (int i = 0; i < 32; ++i) {
		BTEST_ASSERT(barena_malloc(arena, 1000) != NULL);
	}

	// Restoring rewinds the arena: the same request yields the same address
	barena_restore(arena, snapshot);
	BTEST_EXPECT(barena_snapshot(arena) == snapshot);
	void* replay = barena_malloc(arena, 100);
	BTEST_EXPECT(replay == first_after);

	// Chunks freed by the restore are reused instead of being released
	BTEST_EXPECT(fixture.pool.free_chunks != NULL);
	void* reused = barena_malloc(arena, CHUNK_SIZE / 2);
	BTEST_ASSERT(reused != NULL);
	(void)before;
}

BTEST(barena_, reset) {
	barena_t* arena = &fixture.arena;

	void* first = barena_malloc(arena, 100);
	BTEST_ASSERT(first != NULL);
	for (int i = 0; i < 32; ++i) {
		BTEST_ASSERT(barena_malloc(arena, 1000) != NULL);
	}

	barena_reset(arena);
	BTEST_EXPECT(barena_snapshot(arena) == NULL);
	BTEST_EXPECT(arena->current_chunk == NULL);
	BTEST_EXPECT(fixture.pool.free_chunks != NULL);

	// Chunks are handed out again from the pool, the first one being the
	// one that was freed last
	void* again = barena_malloc(arena, 100);
	BTEST_ASSERT(again != NULL);
	BTEST_EXPECT(again == first);
}

BTEST(barena_, shared_pool) {
	// Two arenas can draw from the same pool
	barena_t other;
	barena_init(&other, &fixture.pool);

	void* a = barena_malloc(&fixture.arena, 100);
	void* b = barena_malloc(&other, 100);
	BTEST_ASSERT(a != NULL);
	BTEST_ASSERT(b != NULL);
	BTEST_EXPECT(a != b);

	barena_reset(&other);
	// The chunk released by `other` is now available to `fixture.arena`.
	// A request that no longer fits in its current chunk (which already
	// holds `a`) but still fits in a whole chunk takes it over.
	BTEST_EXPECT(fixture.pool.free_chunks != NULL);
	void* c = barena_malloc(&fixture.arena, fixture.pool.chunk_size - 64);
	BTEST_EXPECT(c == b);
}

#define BLIB_IMPLEMENTATION
#include "../../barena.h"
