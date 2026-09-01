#include "../../bseg.h"
#include "../../btest.h"

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

#define BLIB_IMPLEMENTATION
#include "../../bseg.h"
