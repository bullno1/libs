#ifndef BSEG_H
#define BSEG_H

/**
 * @file
 * @brief Segmented array with stable element pointers.
 *
 * Unlike @ref barray.h, elements are never relocated when the array grows.
 * Storage is a series of segments, each twice the size of its predecessor.
 * Growing allocates a new segment instead of reallocating, so pointers to
 * existing elements remain valid until the array is freed.
 *
 * The tradeoff is that elements are not contiguous so they can only be
 * accessed through @ref bseg_at / @ref bseg_ref instead of plain indexing.
 * Random access is still O(1): locating a segment is a single bit scan.
 *
 * Based on: https://danielchasehooper.com/posts/segment_array/
 *
 * Usage:
 *
 * @code{.c}
 * bseg(int) numbers = { 0 };  // Zero-initialized means empty
 * bseg_push(numbers, 42, NULL);
 * int* first = bseg_ref(numbers, 0);  // Valid until bseg_free
 * bseg_at(numbers, 0) += 1;
 * bseg_free(numbers, NULL);
 * @endcode
 */

#include <stddef.h>

#ifndef BSEG_API
#define BSEG_API
#endif

/**
 * @brief Maximum number of segments.
 *
 * Together with @ref BSEG_SKIPPED_SEGMENTS, the defaults give a maximum
 * capacity of 2^32 - 64 elements.
 */
#ifndef BSEG_MAX_SEGMENTS
#define BSEG_MAX_SEGMENTS 26
#endif

/**
 * @brief How many of the smallest power-of-two segments to skip.
 *
 * The first segment holds `1 << BSEG_SKIPPED_SEGMENTS` elements (64 by
 * default) instead of 1, reducing allocation count for small arrays.
 */
#ifndef BSEG_SKIPPED_SEGMENTS
#define BSEG_SKIPPED_SEGMENTS 6
#endif

/// The underlying untyped segmented array
typedef struct {
	size_t len;
	int num_segments;
	char* segments[BSEG_MAX_SEGMENTS];
} bseg_t;

/**
 * @brief Declare a segmented array of type T.
 *
 * A zero-initialized value is a valid empty array.
 * Use `typedef` to pass it across functions:
 * `typedef bseg(int) int_seg_t;`
 */
#define bseg(T) union { bseg_t bseg; T* bseg__type_hint; }

/// Number of elements in the array
#define bseg_len(seg) ((seg).bseg.len + 0)

/// Number of elements the array can hold without allocating
#define bseg_capacity(seg) bseg__capacity(&(seg).bseg)

/// Pointer to the element at @p index, stable until @ref bseg_free
#define bseg_ref(seg, index) \
	((BSEG__TYPEOF((seg).bseg__type_hint))bseg__at(&(seg).bseg, (index), sizeof(*(seg).bseg__type_hint)))

/// The element at @p index, as an assignable lvalue
#define bseg_at(seg, index) (*bseg_ref(seg, index))

/// Push an element to the end of the array
#define bseg_push(seg, element, ctx) \
	do { \
		BSEG__TYPEOF((seg).bseg__type_hint) bseg__slot = bseg__prepare_push( \
			&(seg).bseg, sizeof(*(seg).bseg__type_hint), (ctx) \
		); \
		*bseg__slot = element; \
	} while (0)

/// Remove the last element and return it
#define bseg_pop(seg) (bseg__do_pop(&(seg).bseg), bseg_at(seg, (seg).bseg.len))

/// Remove the element at @p index by swapping the last element into its place
#define bseg_swap_remove(seg, index) \
	( \
		bseg_at(seg, index) = bseg_at(seg, bseg_len(seg) - 1), \
		bseg_pop(seg) \
	)

/// Ensure the array can hold at least @p new_capacity elements
#define bseg_reserve(seg, new_capacity, ctx) \
	bseg__do_reserve(&(seg).bseg, (new_capacity), sizeof(*(seg).bseg__type_hint), (ctx))

/// Resize the array, zero-initializing any new elements
#define bseg_resize(seg, new_len, ctx) \
	bseg__do_resize(&(seg).bseg, (new_len), sizeof(*(seg).bseg__type_hint), (ctx))

/// Remove all elements, keeping the allocated segments
#define bseg_clear(seg) ((seg).bseg.len = 0)

/// Free all segments; the array becomes empty and can be reused
#define bseg_free(seg, ctx) bseg__do_free(&(seg).bseg, (ctx))

/// Iterate through the array, binding a pointer to each element to @p REF
#define BSEG_FOREACH_REF(REF, SEG) \
	for ( \
		struct { size_t index; char once; } bseg__itr = { 0 }; \
		bseg__itr.index < bseg_len(SEG); \
		++bseg__itr.index \
	) \
		for ( \
			BSEG__TYPEOF((SEG).bseg__type_hint) REF = (bseg__itr.once = 1, bseg_ref(SEG, bseg__itr.index)); \
			bseg__itr.once; \
			bseg__itr.once = 0 \
		)

/// Iterate through the array, binding a copy of each element to @p VALUE
#define BSEG_FOREACH_VALUE(VALUE, SEG) \
	for ( \
		struct { size_t index; char once; } bseg__itr = { 0 }; \
		bseg__itr.index < bseg_len(SEG); \
		++bseg__itr.index \
	) \
		for ( \
			BSEG__TYPEOF(*(SEG).bseg__type_hint) VALUE = (bseg__itr.once = 1, bseg_at(SEG, bseg__itr.index)); \
			bseg__itr.once; \
			bseg__itr.once = 0 \
		)

// Private

BSEG_API size_t
bseg__capacity(const bseg_t* seg);

BSEG_API void*
bseg__at(const bseg_t* seg, size_t index, size_t elem_size);

BSEG_API void*
bseg__prepare_push(bseg_t* seg, size_t elem_size, void* ctx);

BSEG_API void
bseg__do_pop(bseg_t* seg);

BSEG_API void
bseg__do_reserve(bseg_t* seg, size_t new_capacity, size_t elem_size, void* ctx);

BSEG_API void
bseg__do_resize(bseg_t* seg, size_t new_len, size_t elem_size, void* ctx);

BSEG_API void
bseg__do_free(bseg_t* seg, void* ctx);

#if __STDC_VERSION__ >= 202311L
#	define BSEG__TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BSEG__TYPEOF(EXP) __typeof__(EXP)
#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSEG_IMPLEMENTATION)
#define BSEG_IMPLEMENTATION
#endif

#ifdef BSEG_IMPLEMENTATION

#include <string.h>

#ifndef BSEG_REALLOC
#	ifdef BLIB_REALLOC
#		define BSEG_REALLOC BLIB_REALLOC
#	else
#		define BSEG_REALLOC(ptr, size, ctx) bseg__libc_realloc(ptr, size, ctx)
#		define BSEG_USE_LIBC
#	endif
#endif

#ifdef BSEG_USE_LIBC

#include <stdlib.h>

static inline void*
bseg__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static inline int
bseg__log2(size_t x) {
#if defined(__clang__) || defined(__GNUC__)
	return (int)(sizeof(unsigned long long) * 8 - 1) - __builtin_clzll((unsigned long long)x);
#elif defined(_MSC_VER) && defined(_WIN64)
	unsigned long index;
	_BitScanReverse64(&index, (unsigned __int64)x);
	return (int)index;
#elif defined(_MSC_VER)
	unsigned long index;
	_BitScanReverse(&index, (unsigned long)x);
	return (int)index;
#else
	int result = 0;
	while (x >>= 1) { ++result; }
	return result;
#endif
}

static inline size_t
bseg__segment_len(int segment) {
	return ((size_t)1 << BSEG_SKIPPED_SEGMENTS) << segment;
}

static inline size_t
bseg__capacity_for(int num_segments) {
	return bseg__segment_len(num_segments) - ((size_t)1 << BSEG_SKIPPED_SEGMENTS);
}

static inline int
bseg__segment_of(size_t index) {
	return bseg__log2((index >> BSEG_SKIPPED_SEGMENTS) + 1);
}

size_t
bseg__capacity(const bseg_t* seg) {
	return bseg__capacity_for(seg->num_segments);
}

void*
bseg__at(const bseg_t* seg, size_t index, size_t elem_size) {
	int segment = bseg__segment_of(index);
	size_t slot = index - bseg__capacity_for(segment);
	return seg->segments[segment] + slot * elem_size;
}

void*
bseg__prepare_push(bseg_t* seg, size_t elem_size, void* ctx) {
	if (seg->len >= bseg__capacity_for(seg->num_segments)) {
		seg->segments[seg->num_segments] = BSEG_REALLOC(
			NULL, bseg__segment_len(seg->num_segments) * elem_size, ctx
		);
		seg->num_segments += 1;
	}

	size_t index = seg->len++;
	return bseg__at(seg, index, elem_size);
}

void
bseg__do_pop(bseg_t* seg) {
	seg->len -= 1;
}

void
bseg__do_reserve(bseg_t* seg, size_t new_capacity, size_t elem_size, void* ctx) {
	while (bseg__capacity_for(seg->num_segments) < new_capacity) {
		seg->segments[seg->num_segments] = BSEG_REALLOC(
			NULL, bseg__segment_len(seg->num_segments) * elem_size, ctx
		);
		seg->num_segments += 1;
	}
}

void
bseg__do_resize(bseg_t* seg, size_t new_len, size_t elem_size, void* ctx) {
	size_t old_len = seg->len;
	if (new_len > old_len) {
		bseg__do_reserve(seg, new_len, elem_size, ctx);

		// Zero new elements, one segment at a time
		size_t begin = old_len;
		while (begin < new_len) {
			int segment = bseg__segment_of(begin);
			size_t slot = begin - bseg__capacity_for(segment);
			size_t run = bseg__segment_len(segment) - slot;
			if (run > new_len - begin) { run = new_len - begin; }
			memset(seg->segments[segment] + slot * elem_size, 0, run * elem_size);
			begin += run;
		}
	}

	seg->len = new_len;
}

void
bseg__do_free(bseg_t* seg, void* ctx) {
	for (int i = 0; i < seg->num_segments; ++i) {
		BSEG_REALLOC(seg->segments[i], 0, ctx);
	}
	memset(seg, 0, sizeof(*seg));
}

#endif
