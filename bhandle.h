#ifndef BHANDLE_H
#define BHANDLE_H

/**
 * @file
 * @brief Generational handles.
 *
 * A handle is an index paired with a generation counter.
 * The map hands out handles, tells whether one is still live and maps it
 * back to an index.
 * It owns nothing else: object storage belongs to the caller, so any layout
 * works as long as it can be indexed, e.g: a plain array, a @ref barray.h or
 * a @ref bseg.h.
 *
 * Every query returns an index, or a pointer computed from a caller-provided
 * base.
 * A null, stale or out of range handle resolves to -1 or `NULL` and is never
 * an error, including when destroyed.
 *
 * A zero-initialized map is empty.
 * A zero-initialized handle is null.
 *
 * With contiguous storage:
 *
 * @code{.c}
 * bhandle_map_t map = { 0 };
 * barray(texture_t) textures = NULL;
 *
 * bhandle_t h = bhandle_new(&map, NULL);
 * if (barray_len(textures) < bhandle_capacity(&map)) {
 *     barray_resize(textures, bhandle_capacity(&map), NULL);
 * }
 * textures[h.index] = (texture_t){ ... };
 *
 * texture_t* tex = bhandle_ptr(&map, h, textures);  // NULL once destroyed
 *
 * texture_t* dead = bhandle_at(textures, bhandle_destroy(&map, h));
 * if (dead != NULL) { texture_cleanup(dead); }
 * @endcode
 *
 * With segmented storage, only the index is used:
 *
 * @code{.c}
 * bseg(texture_t) textures = { 0 };
 *
 * bhandle_t h = bhandle_new(&map, NULL);
 * bseg_resize(textures, bhandle_capacity(&map), NULL);  // Never relocates
 * bseg_at(textures, h.index) = (texture_t){ ... };
 *
 * ptrdiff_t index = bhandle_index(&map, h);
 * texture_t* tex = index >= 0 ? bseg_ref(textures, index) : NULL;
 * @endcode
 *
 * Capacity only changes in @ref bhandle_new, @ref bhandle_reserve,
 * @ref bhandle_grow and @ref bhandle_load_begin.
 * It doubles when growing, so checking @ref bhandle_capacity after
 * @ref bhandle_new is enough to keep the object storage in sync.
 *
 * ## Saving and loading
 *
 * @ref bhandle_save exposes the internal state and @ref bhandle_load takes it back.
 * The library does no I/O; the host serializes the array however it likes.
 *
 * Objects saved keyed by handle can then be loaded in any order:
 *
 * - **Map first**: every saved handle is already live, so the object
 *   loader only needs @ref bhandle_index.
 *   Capacity is known up front and the object storage is resized once.
 *   This is the recommended order.
 * - **Objects first**, or the map state was never saved: the object
 *   loader calls @ref bhandle_reserve for each handle, which grows the
 *   map as needed and is a no-op for handles that are already live.
 *
 * Without the map state, the generations of slots that were free at save
 * time are lost.
 * A weak reference to a destroyed object can then come back to life once its
 * slot is reused, so save the map state if the objects hold such
 * references.
 *
 * The free list is rebuilt in ascending index order on load, so the sequence
 * of indices handed out after a load may differ from a run that never saved.
 *
 * The width of a generation is part of the saved format, see
 * @ref BHANDLE_GEN_TYPE.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BHANDLE_API
#define BHANDLE_API
#endif

/// Customizable index type, must be unsigned. Also used for capacity and counts.
#ifndef BHANDLE_INDEX_TYPE
#define BHANDLE_INDEX_TYPE uint32_t
#endif

/**
 * @brief Customizable generation type, must be unsigned.
 *
 * Defaults to @ref BHANDLE_INDEX_TYPE.
 * Halving it halves the handle when the index is also halved, at the cost of
 * how many times a slot can be reused before a stale handle could match again.
 */
#ifndef BHANDLE_GEN_TYPE
#define BHANDLE_GEN_TYPE BHANDLE_INDEX_TYPE
#endif

/// Index type used in the library
typedef BHANDLE_INDEX_TYPE bhandle_index_t;

/// Generation type used in the library
typedef BHANDLE_GEN_TYPE bhandle_gen_t;

/**
 * @brief A handle.
 *
 * A zero-initialized handle is always null.
 *
 * The fields are public so the caller can pack, hash, or use `index` directly
 * inside @ref BHANDLE_FOREACH.
 */
typedef struct {
	/// @cond INTERNAL
	bhandle_index_t index;
	bhandle_gen_t gen;
	/// @endcond
} bhandle_t;

/// The null handle
#define BHANDLE_NULL ((bhandle_t){ 0 })

/**
 * @brief Saved map state, a view into the map.
 *
 * Valid until the next mutating call on the map.
 */
typedef struct {
	/// Number of slots
	bhandle_index_t len;
	/// One generation per slot
	const bhandle_gen_t* gens;
} bhandle_state_t;

/// The map. Zero-initialized means empty.
typedef struct {
	/// @cond INTERNAL
	bhandle_index_t len;
	bhandle_index_t count;
	bhandle_index_t free_head;  // Biased by one so that 0 is the empty list
	bool free_dirty;
	bhandle_gen_t* gens;
	bhandle_index_t* next;  // Biased like free_head
	/// @endcond
} bhandle_map_t;

// Lifecycle

/// Release all slot storage; the map becomes empty and can be reused
BHANDLE_API void
bhandle_free(bhandle_map_t* map, void* memctx);

/// Destroy every live handle, keeping capacity. Stale handles stay stale.
BHANDLE_API void
bhandle_clear(bhandle_map_t* map);

/**
 * @brief Ensure at least @p capacity slots without creating any handle.
 *
 * Does nothing and returns false if the allocator refused.
 */
BHANDLE_API bool
bhandle_grow(bhandle_map_t* map, bhandle_index_t capacity, void* memctx);

/// Number of slots. Every index handed out is below this.
BHANDLE_API bhandle_index_t
bhandle_capacity(const bhandle_map_t* map);

/// Number of live handles
BHANDLE_API bhandle_index_t
bhandle_count(const bhandle_map_t* map);

// Create / destroy

/**
 * @brief Create a handle.
 *
 * Reuses a free slot if there is one, otherwise grows.
 * Returns @ref BHANDLE_NULL if it could not grow.
 */
BHANDLE_API bhandle_t
bhandle_new(bhandle_map_t* map, void* ctx);

/**
 * @brief Destroy a handle and return its index so the caller can clean up.
 *
 * Returns -1 and does nothing if @p h is null, stale or out of range.
 * Combine with @ref bhandle_at to get a pointer instead.
 */
BHANDLE_API ptrdiff_t
bhandle_destroy(bhandle_map_t* map, bhandle_t h);

// Resolve

/// Index of a live handle, or -1
BHANDLE_API ptrdiff_t
bhandle_index(const bhandle_map_t* map, bhandle_t h);

/// Whether @p h is live
BHANDLE_API bool
bhandle_is_valid(const bhandle_map_t* map, bhandle_t h);

/// Whether @p h is null, that is, never handed out by any map
static inline bool
bhandle_is_null(bhandle_t h) {
	return (h.gen & 1) == 0;
}

/// Whether two handles are the same
static inline bool
bhandle_eq(bhandle_t lhs, bhandle_t rhs) {
	return lhs.index == rhs.index && lhs.gen == rhs.gen;
}

/**
 * @brief Offset @p base by an index, typed like @p base.
 *
 * `NULL` when @p index is negative, so it composes with every function that
 * returns an index: `bhandle_at(objs, bhandle_destroy(&map, h))`.
 */
#define bhandle_at(base, index) \
	((BHANDLE__TYPEOF(base))bhandle__at((base), (index), sizeof(*(base))))

/// Pointer to the object of a live handle in @p base, or `NULL`
#define bhandle_ptr(map, h, base) \
	bhandle_at(base, bhandle_index((map), (h)))

// Save / load

/// View of the current state, see @ref bhandle_state_t
BHANDLE_API bhandle_state_t
bhandle_save(const bhandle_map_t* map);

/**
 * @brief Replace the map's content with a saved state.
 *
 * Existing content is discarded, the caller must have cleaned it up.
 * Does nothing and returns false if the allocator refused.
 *
 * This copies @p state.
 * See @ref bhandle_load_begin to read directly into the map instead.
 */
BHANDLE_API bool
bhandle_load(bhandle_map_t* map, bhandle_state_t state, void* memctx);

/**
 * @brief Resize to @p len slots and expose the generations for direct writes.
 *
 * Generations up to `min(old, len)` are kept, new ones are zero (free).
 * Returns `NULL` and does nothing if the allocator refused.
 *
 * The map is inconsistent until @ref bhandle_load_end, the only calls
 * allowed in between are @ref bhandle_free and @ref bhandle_load_begin again.
 * A host whose read fails halfway should do one of those, the same as it
 * would for its own object storage.
 */
BHANDLE_API bhandle_gen_t*
bhandle_load_begin(bhandle_map_t* map, bhandle_index_t len, void* memctx);

/// Rebuild the bookkeeping after the generations were written. O(capacity).
BHANDLE_API void
bhandle_load_end(bhandle_map_t* map);

/**
 * @brief Make a specific handle live and return its index.
 *
 * For loading objects that were saved keyed by handle.
 *
 * - Already live with the same generation, e.g: the state was loaded with
 *   @ref bhandle_load. Returns the index, nothing changes.
 * - Slot is free or beyond capacity: grows if needed, marks the slot live
 *   with exactly `h.gen`, returns the index.
 * - Returns -1 if @p h is null, the slot is live under another generation,
 *   or growing failed.
 */
BHANDLE_API ptrdiff_t
bhandle_reserve(bhandle_map_t* map, bhandle_t h, void* ctx);

// Iteration

/**
 * @brief Iterate every live handle, binding it to @p H.
 *
 * Destroying the current handle inside the loop is safe.
 * Handles created inside the loop may or may not be visited.
 */
#define BHANDLE_FOREACH(H, MAP) \
	for ( \
		bhandle_t H = bhandle__next_live((MAP), 0); \
		!bhandle_is_null(H); \
		H = bhandle__next_live((MAP), H.index + 1) \
	)

/**
 * @brief Like @ref BHANDLE_FOREACH, also binding a pointer into @p BASE to @p PTR.
 *
 * @p BASE must be contiguous storage with at least @ref bhandle_capacity
 * elements.
 */
#define BHANDLE_FOREACH_PTR(H, PTR, MAP, BASE) \
	for ( \
		struct { bhandle_t h; int go; } bhandle__itr = { bhandle__next_live((MAP), 0), 1 }; \
		bhandle__itr.go != 0 && !bhandle_is_null(bhandle__itr.h); \
		bhandle__itr.h = bhandle__next_live((MAP), bhandle__itr.h.index + 1), \
		bhandle__itr.go = (bhandle__itr.go == 2) \
	) \
		for (bhandle_t H = bhandle__itr.h; bhandle__itr.go == 1;) \
			for ( \
				BHANDLE__TYPEOF(BASE) PTR = (bhandle__itr.go = 0, (BASE) + H.index); \
				bhandle__itr.go == 0; \
				bhandle__itr.go = 2 \
			)

// Private

BHANDLE_API void*
bhandle__at(const void* base, ptrdiff_t index, size_t item_size);

BHANDLE_API bhandle_t
bhandle__next_live(const bhandle_map_t* map, bhandle_index_t from);

#if __STDC_VERSION__ >= 202311L
#	define BHANDLE__TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BHANDLE__TYPEOF(EXP) __typeof__(EXP)
#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BHANDLE_IMPLEMENTATION)
#define BHANDLE_IMPLEMENTATION
#endif

#ifdef BHANDLE_IMPLEMENTATION

#include <string.h>

#ifndef BHANDLE_REALLOC
#	ifdef BLIB_REALLOC
#		define BHANDLE_REALLOC BLIB_REALLOC
#	else
#		define BHANDLE_REALLOC(ptr, size, ctx) bhandle__libc_realloc(ptr, size, ctx)
#		define BHANDLE_USE_LIBC
#	endif
#endif

#ifdef BHANDLE_USE_LIBC

#include <stdlib.h>

static inline void*
bhandle__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

_Static_assert((bhandle_index_t)-1 > 0, "BHANDLE_INDEX_TYPE must be unsigned");
_Static_assert((bhandle_gen_t)-1 > 0, "BHANDLE_GEN_TYPE must be unsigned");

// The one index a handle can never have, so that `index + 1` always fits in
// the index type. That is what the free list stores, 0 being the end of it.
#define BHANDLE__NIL ((bhandle_index_t)-1)
#define BHANDLE__MIN_CAPACITY 16

static inline bool
bhandle__is_live(bhandle_gen_t gen) {
	return (gen & 1) != 0;
}

static void
bhandle__rebuild(bhandle_map_t* map) {
	bhandle_index_t count = 0;
	bhandle_index_t free_head = 0;

	// Walk backward so the free list pops in ascending order
	for (bhandle_index_t i = map->len; i-- > 0;) {
		if (bhandle__is_live(map->gens[i])) {
			++count;
		} else {
			map->next[i] = free_head;
			free_head = i + 1;
		}
	}

	map->count = count;
	map->free_head = free_head;
	map->free_dirty = false;
}

// Reallocate to exactly new_len slots, zeroing new generations.
// Does not touch the free list: the caller decides when to rebuild.
static bool
bhandle__resize(bhandle_map_t* map, bhandle_index_t new_len, void* ctx) {
	bhandle_index_t old_len = map->len;
	if (new_len == old_len) { return true; }

	if (new_len == 0) {
		BHANDLE_REALLOC(map->gens, 0, ctx);
		BHANDLE_REALLOC(map->next, 0, ctx);
		map->gens = NULL;
		map->next = NULL;
		map->len = 0;
		return true;
	}

	size_t gens_size = (size_t)new_len * sizeof(bhandle_gen_t);
	size_t next_size = (size_t)new_len * sizeof(bhandle_index_t);
	if (
		gens_size / sizeof(bhandle_gen_t) != new_len
		|| next_size / sizeof(bhandle_index_t) != new_len
	) {
		return false;
	}

	bhandle_gen_t* gens = BHANDLE_REALLOC(map->gens, gens_size, ctx);
	if (gens == NULL) { return false; }
	map->gens = gens;

	bhandle_index_t* next = BHANDLE_REALLOC(map->next, next_size, ctx);
	// gens is already bigger than len says, which is harmless: nothing reads
	// past len and the next realloc will shrink or grow it again.
	if (next == NULL) { return false; }
	map->next = next;

	if (new_len > old_len) {
		memset(gens + old_len, 0, (size_t)(new_len - old_len) * sizeof(bhandle_gen_t));
	}
	map->len = new_len;
	return true;
}

// Grow to at least min_len, doubling to keep growth amortized
static bool
bhandle__grow(bhandle_map_t* map, bhandle_index_t min_len, void* ctx) {
	bhandle_index_t len = map->len;
	if (min_len <= len) { return true; }

	bhandle_index_t new_len = len < BHANDLE__MIN_CAPACITY ? BHANDLE__MIN_CAPACITY : len;
	while (new_len < min_len) {
		new_len = new_len > BHANDLE__NIL / 2 ? BHANDLE__NIL : new_len * 2;
	}

	if (!bhandle__resize(map, new_len, ctx)) { return false; }
	map->free_dirty = true;
	return true;
}

void
bhandle_free(bhandle_map_t* map, void* ctx) {
	BHANDLE_REALLOC(map->gens, 0, ctx);
	BHANDLE_REALLOC(map->next, 0, ctx);
	memset(map, 0, sizeof(*map));
}

void
bhandle_clear(bhandle_map_t* map) {
	for (bhandle_index_t i = 0; i < map->len; ++i) {
		if (bhandle__is_live(map->gens[i])) {
			map->gens[i] += 1;
		}
	}
	map->count = 0;
	map->free_dirty = true;
}

bool
bhandle_grow(bhandle_map_t* map, bhandle_index_t capacity, void* ctx) {
	return bhandle__grow(map, capacity, ctx);
}

bhandle_index_t
bhandle_capacity(const bhandle_map_t* map) {
	return map->len;
}

bhandle_index_t
bhandle_count(const bhandle_map_t* map) {
	return map->count;
}

bhandle_t
bhandle_new(bhandle_map_t* map, void* ctx) {
	if (map->free_dirty) { bhandle__rebuild(map); }

	if (map->free_head == 0) {
		if (map->len >= BHANDLE__NIL) { return BHANDLE_NULL; }
		if (!bhandle__grow(map, map->len + 1, ctx)) { return BHANDLE_NULL; }
		bhandle__rebuild(map);
	}

	bhandle_index_t index = map->free_head - 1;
	map->free_head = map->next[index];
	map->gens[index] += 1;
	map->count += 1;

	return (bhandle_t){ .index = index, .gen = map->gens[index] };
}

ptrdiff_t
bhandle_destroy(bhandle_map_t* map, bhandle_t h) {
	ptrdiff_t index = bhandle_index(map, h);
	if (index < 0) { return -1; }

	map->gens[index] += 1;
	map->count -= 1;
	if (!map->free_dirty) {
		map->next[index] = map->free_head;
		map->free_head = h.index + 1;
	}

	return index;
}

ptrdiff_t
bhandle_index(const bhandle_map_t* map, bhandle_t h) {
	if (
		bhandle__is_live(h.gen)
		&& h.index < map->len
		&& map->gens[h.index] == h.gen
	) {
		return (ptrdiff_t)h.index;
	} else {
		return -1;
	}
}

bool
bhandle_is_valid(const bhandle_map_t* map, bhandle_t h) {
	return bhandle_index(map, h) >= 0;
}

bhandle_state_t
bhandle_save(const bhandle_map_t* map) {
	return (bhandle_state_t){ .len = map->len, .gens = map->gens };
}

bool
bhandle_load(bhandle_map_t* map, bhandle_state_t state, void* ctx) {
	bhandle_gen_t* gens = bhandle_load_begin(map, state.len, ctx);
	if (gens == NULL && state.len > 0) { return false; }

	if (state.len > 0) {
		memcpy(gens, state.gens, (size_t)state.len * sizeof(bhandle_gen_t));
	}
	bhandle_load_end(map);
	return true;
}

bhandle_gen_t*
bhandle_load_begin(bhandle_map_t* map, bhandle_index_t len, void* ctx) {
	if (!bhandle__resize(map, len, ctx)) { return NULL; }
	map->free_dirty = true;
	return map->gens;
}

void
bhandle_load_end(bhandle_map_t* map) {
	bhandle__rebuild(map);
}

ptrdiff_t
bhandle_reserve(bhandle_map_t* map, bhandle_t h, void* ctx) {
	if (bhandle_is_null(h) || h.index == BHANDLE__NIL) { return -1; }

	if (h.index < map->len) {
		bhandle_gen_t gen = map->gens[h.index];
		if (gen == h.gen) { return (ptrdiff_t)h.index; }
		if (bhandle__is_live(gen)) { return -1; }
	} else {
		if (!bhandle__grow(map, h.index + 1, ctx)) { return -1; }
	}

	map->gens[h.index] = h.gen;
	map->count += 1;
	map->free_dirty = true;
	return (ptrdiff_t)h.index;
}

void*
bhandle__at(const void* base, ptrdiff_t index, size_t item_size) {
	if (index < 0) { return NULL; }
	// The macro casts the result back to the type of base, const included
	union { const void* c; char* m; } ptr = { base };
	return ptr.m + (size_t)index * item_size;
}

bhandle_t
bhandle__next_live(const bhandle_map_t* map, bhandle_index_t from) {
	for (bhandle_index_t i = from; i < map->len; ++i) {
		if (bhandle__is_live(map->gens[i])) {
			return (bhandle_t){ .index = i, .gen = map->gens[i] };
		}
	}
	return BHANDLE_NULL;
}

#endif
