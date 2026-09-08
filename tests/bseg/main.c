#include "../../bseg.h"
#include "../../btest.h"
#include <stdlib.h>
#include <stdint.h>

// Allocation only fails where it can: embedded, or a fixed pool. Desktop is
// killed or swaps instead, so the refusal path needs an allocator that says no
// on purpose. A NULL ctx is the ordinary libc one every other test uses.
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

static btest_suite_t seg_array = {
	.name = "bseg",
};

BTEST(seg_array, order) {
	bseg(int) seg = { 0 };
	for (int i = 0; i < 1000; ++i) {
		bseg_push(seg, i, NULL);
	}

	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)1000);
	for (int i = 0; i < 1000; ++i) {
		BTEST_EXPECT_EQUAL("%d", bseg_at(seg, i), i);
	}

	bseg_free(seg, NULL);
}

BTEST(seg_array, stable_pointers) {
	bseg(int) seg = { 0 };
	bseg_push(seg, 42, NULL);
	int* first = bseg_ref(seg, 0);

	// Force multiple segment allocations
	for (int i = 0; i < 100000; ++i) {
		bseg_push(seg, i, NULL);
	}

	BTEST_EXPECT(first == bseg_ref(seg, 0));
	BTEST_EXPECT_EQUAL("%d", *first, 42);

	bseg_free(seg, NULL);
}

BTEST(seg_array, pop) {
	bseg(int) seg = { 0 };
	bseg_push(seg, 3, NULL);
	bseg_push(seg, 4, NULL);

	BTEST_EXPECT_EQUAL("%d", bseg_pop(seg), 4);
	BTEST_EXPECT_EQUAL("%d", bseg_pop(seg), 3);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);

	bseg_free(seg, NULL);
}

BTEST(seg_array, swap_remove) {
	bseg(int) seg = { 0 };
	for (int i = 0; i < 4; ++i) {
		bseg_push(seg, i, NULL);
	}

	// Like barray_swap_remove, this returns the moved (last) element
	int moved = bseg_swap_remove(seg, 1);
	BTEST_EXPECT_EQUAL("%d", moved, 3);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)3);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 0);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 1), 3);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 2), 2);

	bseg_free(seg, NULL);
}

BTEST(seg_array, resize_must_zero) {
	bseg(char) seg = { 0 };
	bseg_push(seg, 3, NULL);

	// Span multiple segments to exercise per-segment zeroing
	bseg_resize(seg, 1000, NULL);

	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 3);
	for (int i = 1; i < 1000; ++i) {
		BTEST_EXPECT_EQUAL("%d", bseg_at(seg, i), 0);
	}

	bseg_free(seg, NULL);
}

BTEST(seg_array, resize_shrink) {
	bseg(int) seg = { 0 };
	for (int i = 0; i < 100; ++i) {
		bseg_push(seg, i, NULL);
	}

	bseg_resize(seg, 10, NULL);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)10);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 9), 9);

	bseg_free(seg, NULL);
}

BTEST(seg_array, reserve) {
	bseg(int) seg = { 0 };
	bseg_reserve(seg, 1000, NULL);

	BTEST_EXPECT(bseg_capacity(seg) >= 1000);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);

	int* first = bseg_ref(seg, 0);
	for (int i = 0; i < 1000; ++i) {
		bseg_push(seg, i, NULL);
	}
	BTEST_EXPECT(first == bseg_ref(seg, 0));

	bseg_free(seg, NULL);
}

BTEST(seg_array, clear_keeps_capacity) {
	bseg(int) seg = { 0 };
	for (int i = 0; i < 100; ++i) {
		bseg_push(seg, i, NULL);
	}
	size_t capacity = bseg_capacity(seg);

	bseg_clear(seg);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);
	BTEST_EXPECT_EQUAL("%zu", bseg_capacity(seg), capacity);

	bseg_free(seg, NULL);
}

BTEST(seg_array, free_resets_to_empty) {
	bseg(int) seg = { 0 };
	bseg_push(seg, 1, NULL);
	bseg_free(seg, NULL);

	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);
	BTEST_EXPECT_EQUAL("%zu", bseg_capacity(seg), (size_t)0);

	// Reusable after free
	bseg_push(seg, 2, NULL);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 2);
	bseg_free(seg, NULL);
}

BTEST(seg_array, foreach) {
	bseg(int) seg = { 0 };
	for (int i = 0; i < 200; ++i) {
		bseg_push(seg, i, NULL);
	}

	int expected_value = 0;
	BSEG_FOREACH_VALUE(value, seg) {
		BTEST_EXPECT_EQUAL("%d", value, expected_value);
		++expected_value;
	}
	BTEST_EXPECT_EQUAL("%d", expected_value, 200);

	BSEG_FOREACH_REF(ref, seg) {
		*ref += 1;
	}
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 1);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 199), 200);

	bseg_free(seg, NULL);
}

BTEST(seg_array, push_refuses_without_growing) {
	bseg(int) seg = { 0 };
	budget_t budget = { .budget = 0 };

	bseg_push(seg, 42, &budget);

	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);
	BTEST_EXPECT_EQUAL("%zu", bseg_capacity(seg), (size_t)0);

	bseg_free(seg, &budget);
}

BTEST(seg_array, reserve_keeps_what_it_got) {
	bseg(int) seg = { 0 };
	// Enough for the first segment and nothing after it
	budget_t budget = { .budget = 1 };

	bseg_reserve(seg, 100000, &budget);

	// Less than asked for, but every element of it is real
	size_t capacity = bseg_capacity(seg);
	BTEST_ASSERT(capacity > 0);
	BTEST_EXPECT(capacity < 100000);

	for (size_t i = 0; i < capacity; ++i) {
		bseg_push(seg, (int)i, &budget);
	}
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), capacity);
	for (size_t i = 0; i < capacity; ++i) {
		BTEST_EXPECT_EQUAL("%d", bseg_at(seg, i), (int)i);
	}

	// One past it needs a segment the allocator will not give
	bseg_push(seg, -1, &budget);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), capacity);

	// Handing the budget back makes it grow again: the refusal left nothing
	// broken behind
	budget.budget = 1;
	bseg_push(seg, -1, &budget);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), capacity + 1);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, capacity), -1);

	bseg_free(seg, &budget);
}

BTEST(seg_array, resize_leaves_array_intact) {
	bseg(int) seg = { 0 };
	budget_t budget = { .budget = 1 };

	size_t capacity = 0;
	bseg_reserve(seg, 1, &budget);
	capacity = bseg_capacity(seg);
	BTEST_ASSERT(capacity > 0);

	bseg_resize(seg, capacity, &budget);
	for (size_t i = 0; i < capacity; ++i) {
		bseg_at(seg, i) = (int)i;
	}

	// Growing past what is left is a no-op, not a walk through a segment that
	// was never handed over
	bseg_resize(seg, capacity + 10000, &budget);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), capacity);
	for (size_t i = 0; i < capacity; ++i) {
		BTEST_EXPECT_EQUAL("%d", bseg_at(seg, i), (int)i);
	}

	// Shrinking never needs memory, so it still works
	bseg_resize(seg, 1, &budget);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)1);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 0);

	bseg_free(seg, &budget);
}

BTEST(seg_array, huge_request_is_refused) {
	bseg(int) seg = { 0 };
	budget_t budget = { .budget = 0 };

	// More than the segment table could ever describe. It has to stop at the
	// first refusal rather than run the table off its end.
	bseg_reserve(seg, SIZE_MAX, &budget);

	BTEST_EXPECT_EQUAL("%zu", bseg_capacity(seg), (size_t)0);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);

	bseg_free(seg, &budget);
}

BTEST(seg_array, reusable_after_failure) {
	bseg(int) seg = { 0 };
	budget_t budget = { .budget = 0 };

	bseg_push(seg, 1, &budget);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)0);

	bseg_free(seg, &budget);

	// A plain libc ctx: the array survived the refusal well enough to be used
	bseg_push(seg, 2, NULL);
	BTEST_EXPECT_EQUAL("%zu", bseg_len(seg), (size_t)1);
	BTEST_EXPECT_EQUAL("%d", bseg_at(seg, 0), 2);

	bseg_free(seg, NULL);
}

#define BSEG_REALLOC(ptr, size, ctx) test_realloc(ptr, size, ctx)
#define BLIB_IMPLEMENTATION
#include "../../bseg.h"
