#include "../../bhandle.h"
#include "../../bseg.h"
#include "../../btest.h"
#include <stdlib.h>
#include <string.h>

// An allocator that says no on purpose, see tests/bseg for the rationale
typedef struct {
	int budget;
} budget_t;

static void*
test_realloc(void* ptr, size_t size, void* ctx) {
	budget_t* budget = ctx;

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	if (budget != NULL) {
		if (budget->budget <= 0) { return NULL; }
		--budget->budget;
	}

	return realloc(ptr, size);
}

typedef struct {
	int value;
} object_t;

static btest_suite_t handle = {
	.name = "bhandle",
};

BTEST(handle, null_handle) {
	bhandle_map_t map = { 0 };
	bhandle_t null = { 0 };

	BTEST_EXPECT(bhandle_is_null(null));
	BTEST_EXPECT(bhandle_is_null(BHANDLE_NULL));
	BTEST_EXPECT(bhandle_eq(null, BHANDLE_NULL));

	// Against an empty manager, nothing is allocated
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, null), (ptrdiff_t)-1);
	BTEST_EXPECT(!bhandle_is_valid(&map, null));
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, null), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);

	// Against a manager with a free slot 0: a null handle must not match it
	bhandle_t h = bhandle_new(&map, NULL);
	bhandle_destroy(&map, h);
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, null), (ptrdiff_t)-1);

	bhandle_free(&map, NULL);
}

BTEST(handle, create_resolve_destroy) {
	bhandle_map_t map = { 0 };

	bhandle_t h = bhandle_new(&map, NULL);
	BTEST_ASSERT(!bhandle_is_null(h));
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 1u);
	BTEST_EXPECT(bhandle_capacity(&map) > h.index);

	ptrdiff_t index = bhandle_index(&map, h);
	BTEST_EXPECT(index >= 0);
	BTEST_EXPECT_EQUAL("%td", index, (ptrdiff_t)h.index);
	BTEST_EXPECT(bhandle_is_valid(&map, h));

	// Destroy returns the same index, then the handle is stale
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, h), index);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, h), (ptrdiff_t)-1);
	BTEST_EXPECT(!bhandle_is_valid(&map, h));

	// Destroying again is not an error
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, h), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);

	bhandle_free(&map, NULL);
}

BTEST(handle, stale_after_reuse) {
	bhandle_map_t map = { 0 };

	bhandle_t old = bhandle_new(&map, NULL);
	bhandle_destroy(&map, old);

	// The slot is reused but the generation moved on
	bhandle_t new = bhandle_new(&map, NULL);
	BTEST_EXPECT_EQUAL("%u", new.index, old.index);
	BTEST_EXPECT(new.gen != old.gen);
	BTEST_EXPECT(!bhandle_eq(new, old));

	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, old), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, new), (ptrdiff_t)new.index);

	// A stale handle cannot destroy the new occupant
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, old), (ptrdiff_t)-1);
	BTEST_EXPECT(bhandle_is_valid(&map, new));

	bhandle_free(&map, NULL);
}

BTEST(handle, out_of_range) {
	bhandle_map_t map = { 0 };
	bhandle_new(&map, NULL);

	bhandle_t forged = { .index = bhandle_capacity(&map) + 100, .gen = 1 };
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, forged), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, forged), (ptrdiff_t)-1);

	bhandle_free(&map, NULL);
}

BTEST(handle, unique_indices) {
	bhandle_map_t map = { 0 };
	enum { N = 1000 };
	bhandle_t handles[N];

	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
		BTEST_ASSERT(!bhandle_is_null(handles[i]));
	}
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), (unsigned)N);

	// Every live handle has a distinct index below capacity
	bhandle_index_t capacity = bhandle_capacity(&map);
	char* seen = calloc(capacity, 1);
	for (int i = 0; i < N; ++i) {
		BTEST_ASSERT(handles[i].index < capacity);
		BTEST_EXPECT(!seen[handles[i].index]);
		seen[handles[i].index] = 1;
	}

	// Free every other one and create again: reuse never collides
	for (int i = 0; i < N; i += 2) {
		bhandle_destroy(&map, handles[i]);
		seen[handles[i].index] = 0;
	}
	for (int i = 0; i < N; i += 2) {
		handles[i] = bhandle_new(&map, NULL);
		BTEST_ASSERT(handles[i].index < capacity);
		BTEST_EXPECT(!seen[handles[i].index]);
		seen[handles[i].index] = 1;
	}
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), (unsigned)N);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), capacity);

	free(seen);
	bhandle_free(&map, NULL);
}

BTEST(handle, contiguous_storage) {
	bhandle_map_t map = { 0 };
	object_t* objects = NULL;
	size_t len = 0;

	bhandle_t h = bhandle_new(&map, NULL);
	if (len < bhandle_capacity(&map)) {
		len = bhandle_capacity(&map);
		objects = realloc(objects, len * sizeof(object_t));
	}
	objects[h.index].value = 42;

	object_t* obj = bhandle_ptr(&map, h, objects);
	BTEST_ASSERT(obj != NULL);
	BTEST_EXPECT(obj == &objects[h.index]);
	BTEST_EXPECT_EQUAL("%d", obj->value, 42);

	// bhandle_at composes with destroy, and const is preserved
	const object_t* const_objects = objects;
	const object_t* dead = bhandle_at(const_objects, bhandle_destroy(&map, h));
	BTEST_EXPECT(dead == obj);
	BTEST_EXPECT_EQUAL("%d", dead->value, 42);

	BTEST_EXPECT(bhandle_ptr(&map, h, objects) == NULL);
	BTEST_EXPECT(bhandle_at(objects, bhandle_destroy(&map, h)) == NULL);
	BTEST_EXPECT(bhandle_at(objects, (ptrdiff_t)-1) == NULL);

	free(objects);
	bhandle_free(&map, NULL);
}

BTEST(handle, segmented_storage) {
	bhandle_map_t map = { 0 };
	bseg(object_t) objects = { 0 };
	enum { N = 5000 };
	bhandle_t handles[N];

	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
		bseg_resize(objects, bhandle_capacity(&map), NULL);
		bseg_at(objects, handles[i].index).value = i;
	}

	// Pointers stay valid even though the manager grew many times
	object_t* first = bseg_ref(objects, handles[0].index);
	for (int i = 0; i < N; ++i) {
		ptrdiff_t index = bhandle_index(&map, handles[i]);
		BTEST_ASSERT(index >= 0);
		BTEST_EXPECT_EQUAL("%d", bseg_ref(objects, (size_t)index)->value, i);
	}
	BTEST_EXPECT(first == bseg_ref(objects, handles[0].index));

	bseg_free(objects, NULL);
	bhandle_free(&map, NULL);
}

BTEST(handle, clear) {
	bhandle_map_t map = { 0 };
	bhandle_t a = bhandle_new(&map, NULL);
	bhandle_t b = bhandle_new(&map, NULL);
	bhandle_index_t capacity = bhandle_capacity(&map);

	bhandle_clear(&map);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), capacity);
	BTEST_EXPECT(!bhandle_is_valid(&map, a));
	BTEST_EXPECT(!bhandle_is_valid(&map, b));

	// Slots are reusable and old handles stay stale
	bhandle_t c = bhandle_new(&map, NULL);
	BTEST_EXPECT(bhandle_is_valid(&map, c));
	BTEST_EXPECT(!bhandle_is_valid(&map, a));
	BTEST_EXPECT(!bhandle_is_valid(&map, b));
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 1u);

	bhandle_free(&map, NULL);
}

BTEST(handle, free_resets_to_empty) {
	bhandle_map_t map = { 0 };
	bhandle_t h = bhandle_new(&map, NULL);
	bhandle_free(&map, NULL);

	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);
	BTEST_EXPECT(!bhandle_is_valid(&map, h));

	// Reusable after free
	h = bhandle_new(&map, NULL);
	BTEST_EXPECT(bhandle_is_valid(&map, h));
	bhandle_free(&map, NULL);
}

BTEST(handle, grow) {
	bhandle_map_t map = { 0 };

	BTEST_EXPECT(bhandle_grow(&map, 1000, NULL));
	BTEST_EXPECT(bhandle_capacity(&map) >= 1000);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);

	// Creating within the reserved capacity does not grow
	bhandle_index_t capacity = bhandle_capacity(&map);
	for (int i = 0; i < 1000; ++i) {
		BTEST_ASSERT(!bhandle_is_null(bhandle_new(&map, NULL)));
	}
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), capacity);

	// Shrinking is a no-op
	BTEST_EXPECT(bhandle_grow(&map, 1, NULL));
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), capacity);

	bhandle_free(&map, NULL);
}

BTEST(handle, foreach) {
	bhandle_map_t map = { 0 };
	enum { N = 100 };
	bhandle_t handles[N];
	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
	}
	for (int i = 0; i < N; i += 3) {
		bhandle_destroy(&map, handles[i]);
	}

	// Visits exactly the live handles, in index order
	int visited = 0;
	bhandle_index_t last_index = 0;
	BHANDLE_FOREACH(h, &map) {
		BTEST_EXPECT(bhandle_is_valid(&map, h));
		BTEST_EXPECT(visited == 0 || h.index > last_index);
		last_index = h.index;
		++visited;
	}
	BTEST_EXPECT_EQUAL("%d", visited, (int)bhandle_count(&map));

	// Destroying the current handle mid-loop is safe
	visited = 0;
	BHANDLE_FOREACH(h, &map) {
		++visited;
		bhandle_destroy(&map, h);
	}
	BTEST_EXPECT_EQUAL("%d", visited, N - (N + 2) / 3);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);

	// Empty manager: no iteration
	visited = 0;
	BHANDLE_FOREACH(h, &map) {
		(void)h;
		++visited;
	}
	BTEST_EXPECT_EQUAL("%d", visited, 0);

	bhandle_free(&map, NULL);
}

BTEST(handle, foreach_ptr) {
	bhandle_map_t map = { 0 };
	enum { N = 50 };
	bhandle_t handles[N];
	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
	}
	object_t* objects = calloc(bhandle_capacity(&map), sizeof(object_t));
	for (int i = 0; i < N; ++i) {
		objects[handles[i].index].value = i;
	}
	for (int i = 0; i < N; i += 2) {
		bhandle_destroy(&map, handles[i]);
	}

	int visited = 0;
	BHANDLE_FOREACH_PTR(h, obj, &map, objects) {
		BTEST_EXPECT(obj == &objects[h.index]);
		bool odd = (obj->value & 1) != 0;
		BTEST_EXPECT(odd);
		obj->value = -obj->value;
		++visited;
	}
	BTEST_EXPECT_EQUAL("%d", visited, N / 2);
	for (int i = 1; i < N; i += 2) {
		BTEST_EXPECT_EQUAL("%d", objects[handles[i].index].value, -i);
	}

	// break and continue behave like in a plain loop
	visited = 0;
	BHANDLE_FOREACH_PTR(h, obj, &map, objects) {
		(void)h;
		++visited;
		if (visited == 1) { continue; }
		if (obj->value == -5) { break; }
	}
	BTEST_EXPECT_EQUAL("%d", visited, 3);

	free(objects);
	bhandle_free(&map, NULL);
}

BTEST(handle, save_load) {
	bhandle_map_t map = { 0 };
	enum { N = 40 };
	bhandle_t handles[N];
	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
	}
	bhandle_t stale[N / 2];
	for (int i = 0; i < N / 2; ++i) {
		stale[i] = handles[i * 2];
		bhandle_destroy(&map, stale[i]);
	}
	// A reused slot: its previous handle must stay stale after a round trip
	bhandle_t reused = bhandle_new(&map, NULL);

	// The host owns the copy
	bhandle_state_t state = bhandle_save(&map);
	BTEST_ASSERT(state.len == bhandle_capacity(&map));
	bhandle_gen_t* copy = malloc(state.len * sizeof(bhandle_gen_t));
	memcpy(copy, state.gens, state.len * sizeof(bhandle_gen_t));
	bhandle_free(&map, NULL);

	bhandle_map_t loaded = { 0 };
	bhandle_new(&loaded, NULL);  // Pre-existing content is discarded
	BTEST_ASSERT(bhandle_load(&loaded, (bhandle_state_t){ .len = state.len, .gens = copy }, NULL));
	free(copy);

	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&loaded), state.len);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&loaded), (unsigned)(N / 2 + 1));
	for (int i = 1; i < N; i += 2) {
		BTEST_EXPECT_EQUAL("%td", bhandle_index(&loaded, handles[i]), (ptrdiff_t)handles[i].index);
	}
	BTEST_EXPECT(bhandle_is_valid(&loaded, reused));
	for (int i = 0; i < N / 2; ++i) {
		BTEST_EXPECT(!bhandle_is_valid(&loaded, stale[i]));
	}

	// New handles never collide with loaded ones, and stale ones stay stale
	for (int i = 0; i < N; ++i) {
		bhandle_t h = bhandle_new(&loaded, NULL);
		BTEST_ASSERT(!bhandle_is_null(h));
		for (int j = 1; j < N; j += 2) {
			BTEST_EXPECT(h.index != handles[j].index);
		}
		for (int j = 0; j < N / 2; ++j) {
			BTEST_EXPECT(!bhandle_eq(h, stale[j]));
		}
	}
	for (int i = 0; i < N / 2; ++i) {
		BTEST_EXPECT(!bhandle_is_valid(&loaded, stale[i]));
	}

	bhandle_free(&loaded, NULL);
}

BTEST(handle, load_empty) {
	bhandle_map_t map = { 0 };
	bhandle_new(&map, NULL);

	BTEST_EXPECT(bhandle_load(&map, (bhandle_state_t){ 0 }, NULL));
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);

	// Usable afterwards
	BTEST_EXPECT(bhandle_is_valid(&map, bhandle_new(&map, NULL)));
	bhandle_free(&map, NULL);
}

BTEST(handle, load_zero_copy) {
	bhandle_map_t map = { 0 };
	bhandle_t a = bhandle_new(&map, NULL);
	bhandle_t b = bhandle_new(&map, NULL);
	bhandle_t c = bhandle_new(&map, NULL);
	bhandle_destroy(&map, b);

	bhandle_state_t state = bhandle_save(&map);
	bhandle_gen_t* copy = malloc(state.len * sizeof(bhandle_gen_t));
	memcpy(copy, state.gens, state.len * sizeof(bhandle_gen_t));
	bhandle_index_t len = state.len;

	// Read directly into the manager, shrinking it on the way
	bhandle_free(&map, NULL);
	bhandle_grow(&map, len * 4, NULL);
	bhandle_gen_t* gens = bhandle_load_begin(&map, len, NULL);
	BTEST_ASSERT(gens != NULL);
	for (bhandle_index_t i = 0; i < len; ++i) {
		gens[i] = copy[i];
	}
	bhandle_load_end(&map);
	free(copy);

	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), len);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 2u);
	BTEST_EXPECT(bhandle_is_valid(&map, a));
	BTEST_EXPECT(!bhandle_is_valid(&map, b));
	BTEST_EXPECT(bhandle_is_valid(&map, c));

	// Growing through load_begin keeps what was there and frees the rest
	gens = bhandle_load_begin(&map, len * 2, NULL);
	BTEST_ASSERT(gens != NULL);
	bhandle_load_end(&map);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), len * 2);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 2u);
	BTEST_EXPECT(bhandle_is_valid(&map, a));
	BTEST_EXPECT(bhandle_is_valid(&map, c));
	bhandle_t d = bhandle_new(&map, NULL);
	BTEST_EXPECT(bhandle_is_valid(&map, d));
	BTEST_EXPECT(!bhandle_eq(d, b));

	bhandle_free(&map, NULL);
}

BTEST(handle, reserve) {
	bhandle_map_t map = { 0 };

	// Null is refused
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, BHANDLE_NULL, NULL), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);

	// Beyond capacity: grows and marks live with exactly that generation
	bhandle_t far = { .index = 1000, .gen = 7 };
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, far, NULL), (ptrdiff_t)1000);
	BTEST_EXPECT(bhandle_capacity(&map) > 1000);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 1u);
	BTEST_EXPECT(bhandle_is_valid(&map, far));
	BTEST_EXPECT_EQUAL("%td", bhandle_index(&map, far), (ptrdiff_t)1000);

	// Idempotent
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, far, NULL), (ptrdiff_t)1000);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 1u);

	// Same slot under another generation is a conflict
	bhandle_t conflict = { .index = 1000, .gen = 9 };
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, conflict, NULL), (ptrdiff_t)-1);
	BTEST_EXPECT(bhandle_is_valid(&map, far));
	BTEST_EXPECT(!bhandle_is_valid(&map, conflict));

	// A free slot within capacity takes whatever generation is asked
	bhandle_t near = { .index = 3, .gen = 101 };
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, near, NULL), (ptrdiff_t)3);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 2u);

	// Creating afterwards never hands out a reserved slot
	for (int i = 0; i < 2000; ++i) {
		bhandle_t h = bhandle_new(&map, NULL);
		BTEST_ASSERT(!bhandle_is_null(h));
		BTEST_EXPECT(h.index != 3 && h.index != 1000);
	}
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 2002u);
	BTEST_EXPECT(bhandle_is_valid(&map, far));
	BTEST_EXPECT(bhandle_is_valid(&map, near));

	// Destroying a reserved handle works like any other
	BTEST_EXPECT_EQUAL("%td", bhandle_destroy(&map, near), (ptrdiff_t)3);
	BTEST_EXPECT(!bhandle_is_valid(&map, near));

	bhandle_free(&map, NULL);
}

BTEST(handle, reserve_after_load_is_noop) {
	bhandle_map_t map = { 0 };
	bhandle_t a = bhandle_new(&map, NULL);
	bhandle_t b = bhandle_new(&map, NULL);
	bhandle_destroy(&map, a);

	bhandle_state_t state = bhandle_save(&map);
	bhandle_map_t loaded = { 0 };
	BTEST_ASSERT(bhandle_load(&loaded, state, NULL));
	bhandle_free(&map, NULL);

	// Live at save time: already there
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&loaded, b, NULL), (ptrdiff_t)b.index);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&loaded), 1u);

	// Stale at save time: its slot is free, so reserving it is granted with
	// exactly the generation asked for. Deciding whether that is desirable is
	// the host's call.
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&loaded, a, NULL), (ptrdiff_t)a.index);
	BTEST_EXPECT(bhandle_is_valid(&loaded, a));

	bhandle_free(&loaded, NULL);
}

BTEST(handle, reserve_then_load_matches_load_then_reserve) {
	// Two hosts: one loads the manager first, the other reserves as it goes.
	// Both end with the same live set.
	bhandle_map_t map = { 0 };
	enum { N = 30 };
	bhandle_t handles[N];
	for (int i = 0; i < N; ++i) {
		handles[i] = bhandle_new(&map, NULL);
	}
	for (int i = 0; i < N; i += 4) {
		bhandle_destroy(&map, handles[i]);
	}
	bhandle_state_t state = bhandle_save(&map);

	bhandle_map_t first = { 0 };
	BTEST_ASSERT(bhandle_load(&first, state, NULL));
	for (int i = 0; i < N; ++i) {
		if (i % 4 == 0) { continue; }
		BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&first, handles[i], NULL), (ptrdiff_t)handles[i].index);
	}

	bhandle_map_t second = { 0 };
	for (int i = 0; i < N; ++i) {
		if (i % 4 == 0) { continue; }
		BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&second, handles[i], NULL), (ptrdiff_t)handles[i].index);
	}
	BTEST_ASSERT(bhandle_load(&second, state, NULL));

	BTEST_EXPECT_EQUAL("%u", bhandle_count(&first), bhandle_count(&second));
	for (int i = 0; i < N; ++i) {
		BTEST_EXPECT(bhandle_is_valid(&first, handles[i]) == bhandle_is_valid(&second, handles[i]));
		BTEST_EXPECT(bhandle_is_valid(&first, handles[i]) == (i % 4 != 0));
	}

	bhandle_free(&first, NULL);
	bhandle_free(&second, NULL);
	bhandle_free(&map, NULL);
}

BTEST(handle, new_refuses_without_growing) {
	bhandle_map_t map = { 0 };
	budget_t budget = { .budget = 0 };

	bhandle_t h = bhandle_new(&map, &budget);
	BTEST_EXPECT(bhandle_is_null(h));
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 0u);

	// Handing the budget back makes it work: the refusal left nothing broken
	budget.budget = 2;
	h = bhandle_new(&map, &budget);
	BTEST_EXPECT(bhandle_is_valid(&map, h));

	// Half a growth (only one of the two arrays) is also survivable
	bhandle_index_t capacity = bhandle_capacity(&map);
	for (bhandle_index_t i = 1; i < capacity; ++i) {
		BTEST_ASSERT(!bhandle_is_null(bhandle_new(&map, &budget)));
	}
	budget.budget = 1;
	BTEST_EXPECT(bhandle_is_null(bhandle_new(&map, &budget)));
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), capacity);
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), capacity);
	BTEST_EXPECT(bhandle_is_valid(&map, h));

	budget.budget = 2;
	BTEST_EXPECT(!bhandle_is_null(bhandle_new(&map, &budget)));
	BTEST_EXPECT(bhandle_capacity(&map) > capacity);

	bhandle_free(&map, &budget);
}

BTEST(handle, reserve_and_load_refuse) {
	bhandle_map_t map = { 0 };
	budget_t budget = { .budget = 0 };

	bhandle_t far = { .index = 100, .gen = 1 };
	BTEST_EXPECT_EQUAL("%td", bhandle_reserve(&map, far, &budget), (ptrdiff_t)-1);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);

	bhandle_gen_t gens[4] = { 1, 0, 3, 2 };
	bhandle_state_t state = { .len = 4, .gens = gens };
	BTEST_EXPECT(!bhandle_load(&map, state, &budget));
	BTEST_EXPECT(bhandle_load_begin(&map, 4, &budget) == NULL);
	BTEST_EXPECT_EQUAL("%u", bhandle_capacity(&map), 0u);

	budget.budget = 2;
	BTEST_EXPECT(bhandle_load(&map, state, &budget));
	BTEST_EXPECT_EQUAL("%u", bhandle_count(&map), 2u);
	BTEST_EXPECT(bhandle_is_valid(&map, (bhandle_t){ .index = 0, .gen = 1 }));
	BTEST_EXPECT(bhandle_is_valid(&map, (bhandle_t){ .index = 2, .gen = 3 }));
	BTEST_EXPECT(!bhandle_is_valid(&map, (bhandle_t){ .index = 1, .gen = 1 }));
	BTEST_EXPECT(!bhandle_is_valid(&map, (bhandle_t){ .index = 3, .gen = 1 }));

	bhandle_free(&map, &budget);
}

#define BHANDLE_REALLOC(ptr, size, ctx) test_realloc(ptr, size, ctx)
#define BLIB_IMPLEMENTATION
#include "../../bhandle.h"
