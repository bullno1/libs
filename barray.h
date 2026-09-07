#ifndef BARRAY_H
#define BARRAY_H

/**
 * @file
 * @brief Dynamic array.
 *
 * A `barray(T)` is a plain `T*`: elements are indexed with `[]` and the
 * length and capacity live in a header stored right before them.
 * `NULL` is a valid empty array so no initialization is needed:
 *
 * ```c
 * barray(int) numbers = NULL;
 * barray_push(numbers, 42, NULL);
 * for (size_t i = 0; i < barray_len(numbers); ++i) { ... }
 * barray_free(numbers, NULL);
 * ```
 *
 * The macros that can grow or free the array (@ref barray_push,
 * @ref barray_reserve, @ref barray_resize, @ref barray_free) reassign the
 * variable passed to them, so it must be an lvalue.
 * Elements move when the array grows: pointers into it are only valid until
 * the next such call.
 * See @ref bseg.h for an array with stable element pointers.
 *
 * Memory is allocated with `BARRAY_REALLOC` (or `BLIB_REALLOC`), which
 * defaults to libc, and every allocating macro takes a `ctx` argument that
 * is passed through to it (see @ref allocator).
 * Elements are aligned to `BARRAY_ALIGN_TYPE`, `max_align_t` by default.
 *
 * In **exactly one** source file, define `BARRAY_IMPLEMENTATION` before including barray.h.
 */

#include <stddef.h>
#include <string.h>

#ifndef BARRAY_API
#define BARRAY_API
#endif

/**
 * The type of a dynamic array of `T`, which is just `T*`.
 *
 * @hideinitializer
 */
#define barray(T) T*

/**
 * Append an element, growing the array if needed.
 *
 * The capacity doubles when exhausted so pushes are amortized constant time.
 *
 * @param array the array, reassigned if it grows
 * @param element the element to append
 * @param ctx memory context
 *
 * @hideinitializer
 */
#define barray_push(array, element, ctx) \
	do { \
		size_t barray__new_len; \
		(array) = barray__prepare_push((array), &barray__new_len, sizeof(*(array)), (ctx)); \
		(array)[barray__new_len - 1] = element; \
	} while (0)

/**
 * Ensure the array can hold at least `new_capacity` elements.
 *
 * The length is unchanged and the capacity never shrinks.
 *
 * @param array the array, reassigned if it grows
 * @param new_capacity the minimum capacity
 * @param ctx memory context
 *
 * @hideinitializer
 */
#define barray_reserve(array, new_capacity, ctx) \
	do { \
		(array) = barray__do_reserve((array), new_capacity, sizeof(*(array)), (ctx)); \
	} while (0)

/**
 * Remove an element, shifting the following ones down to keep their order.
 *
 * This is linear in the number of elements after `index`, see
 * @ref barray_swap_remove for a constant time alternative.
 * The removed element is not returned.
 *
 * @param array the array
 * @param index index of the element to remove
 *
 * @hideinitializer
 */
#define barray_shift_remove(array, index) \
	( \
		memmove(&(array)[index], &(array)[index + 1], ((int)barray_len((array)) - index - 1) * sizeof(*(array))), \
		barray_pop(array) \
	)

/**
 * Remove an element by moving the last one into its place.
 *
 * Constant time but the order of the elements is not preserved.
 * The removed element is not returned.
 *
 * @param array the array
 * @param index index of the element to remove
 *
 * @hideinitializer
 */
#define barray_swap_remove(array, index) \
	( \
		(array)[index] = (array)[barray_len((array)) - 1], \
		barray_pop(array) \
	)

/**
 * Set the length of the array.
 *
 * New elements are zero-initialized.
 * Shrinking keeps the capacity.
 *
 * @param array the array, reassigned if it grows
 * @param new_len the new length
 * @param ctx memory context
 *
 * @hideinitializer
 */
#define barray_resize(array, new_len, ctx) \
	do { \
		(array) = barray__do_resize((array), new_len, sizeof(*(array)), (ctx)); \
	} while (0)

/**
 * Release the array and set the variable to NULL.
 *
 * Safe to call on an empty (NULL) array.
 *
 * @param array the array
 * @param ctx memory context
 *
 * @hideinitializer
 */
#define barray_free(array, ctx) \
	do { \
		barray__do_free((array), (ctx)); \
		(array) = NULL; \
	} while (0)

/**
 * Remove the last element and return it.
 *
 * The array must not be empty.
 *
 * @param array the array
 *
 * @hideinitializer
 */
#define barray_pop(array) (barray__do_pop((array)), array[barray_len((array))])

/**
 * Iterate over the array with a pointer to each element.
 *
 * The array must not be modified during iteration.
 * Requires `typeof` (C23, GCC, Clang or MSVC).
 *
 * @param REF name of the pointer variable
 * @param ARRAY the array
 *
 * @hideinitializer
 */
#define BARRAY_FOREACH_REF(REF, ARRAY) \
	for ( \
		struct { size_t index; char once; } barray__itr = { 0 }; \
		barray__itr.index < barray_len(ARRAY); \
		++barray__itr.index \
	) \
		for ( \
			BARRAY__TYPEOF(ARRAY) REF = (barray__itr.once = 1, &(ARRAY)[barray__itr.index]); \
			barray__itr.once; \
			barray__itr.once = 0 \
		)

/**
 * Iterate over the array with a copy of each element.
 *
 * The array must not be modified during iteration.
 * Requires `typeof` (C23, GCC, Clang or MSVC).
 *
 * @param VALUE name of the element variable
 * @param ARRAY the array
 *
 * @hideinitializer
 */
#define BARRAY_FOREACH_VALUE(VALUE, ARRAY) \
	for ( \
		struct { size_t index; char once; } barray__itr = { 0 }; \
		barray__itr.index < barray_len(ARRAY); \
		++barray__itr.index \
	) \
		for ( \
			BARRAY__TYPEOF(*ARRAY) VALUE = (barray__itr.once = 1, (ARRAY)[barray__itr.index]); \
			barray__itr.once; \
			barray__itr.once = 0 \
		)

/*! Number of elements in the array, 0 for NULL */
BARRAY_API size_t
barray_len(void* array);

/*! Number of elements the array can hold before growing, 0 for NULL */
BARRAY_API size_t
barray_capacity(void* array);

/*! Remove all elements, keeping the capacity */
BARRAY_API void
barray_clear(void* array);

// Private

BARRAY_API void*
barray__prepare_push(void* array, size_t* new_len, size_t elem_size, void* ctx);

BARRAY_API void*
barray__do_reserve(void* array, size_t new_capacity, size_t elem_size, void* ctx);

BARRAY_API void*
barray__do_resize(void* array, size_t new_len, size_t elem_size, void* ctx) ;

BARRAY_API void
barray__do_pop(void* array);

BARRAY_API void
barray__do_free(void* array, void* ctx);

#if __STDC_VERSION__ >= 202311L
#	define BARRAY__TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BARRAY__TYPEOF(EXP) __typeof__(EXP)
#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BARRAY_IMPLEMENTATION)
#define BARRAY_IMPLEMENTATION
#endif

#ifdef BARRAY_IMPLEMENTATION

#include <string.h>

#ifndef BARRAY_ALIGN_TYPE
#	ifdef _MSC_VER
#		define BARRAY_ALIGN_TYPE long double
#	else
#		define BARRAY_ALIGN_TYPE max_align_t
#	endif
#endif

#ifndef BARRAY_REALLOC
#	ifdef BLIB_REALLOC
#		define BARRAY_REALLOC BLIB_REALLOC
#	else
#		define BARRAY_REALLOC(ptr, size, ctx) barray__libc_realloc(ptr, size, ctx)
#		define BARRAY_USE_LIBC
#	endif
#endif

#ifdef BARRAY_USE_LIBC

#include <stdlib.h>

static inline void*
barray__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

typedef struct {
	size_t capacity;
	size_t len;
	_Alignas(BARRAY_ALIGN_TYPE) char elems[];
} barray_header_t;

static inline barray_header_t*
barray__header_of(void* array) {
	if (array != NULL) {
		return (barray_header_t*)((char*)array - offsetof(barray_header_t, elems));
	} else {
		return NULL;
	}
}

size_t
barray_len(void* array) {
	barray_header_t* header = barray__header_of(array);
	return header != NULL ? header->len : 0;
}

size_t
barray_capacity(void* array) {
	barray_header_t* header = barray__header_of(array);
	return header != NULL ? header->capacity : 0;
}

void
barray__do_free(void* array, void* ctx) {
	barray_header_t* header = barray__header_of(array);
	if (header != NULL) {
		BARRAY_REALLOC(header, 0, ctx);
	}
}

void
barray_clear(void* array) {
	barray_header_t* header = barray__header_of(array);
	if (header != NULL) {
		header->len = 0;
	}
}

void*
barray__prepare_push(void* array, size_t* new_len, size_t elem_size, void* ctx) {
	barray_header_t* header = barray__header_of(array);
	size_t len = header != NULL ? header->len : 0;
	size_t capacity = header != NULL ? header->capacity : 0;

	if (len < capacity) {
		header->len = *new_len = len + 1;
		return array;
	} else {
		size_t new_capacity = capacity > 0 ? capacity * 2 : 2;
		barray_header_t* new_header = BARRAY_REALLOC(
			header, sizeof(barray_header_t) + elem_size * new_capacity, ctx
		);
		new_header->capacity = new_capacity;
		new_header->len = *new_len = len + 1;
		return new_header->elems;
	}
}

void*
barray__do_reserve(void* array, size_t new_capacity, size_t elem_size, void* ctx) {
	barray_header_t* header = barray__header_of(array);
	size_t current_capacity = header != NULL ? header->capacity : 0;
	if (new_capacity <= current_capacity) {
		return array;
	}

	barray_header_t* new_header = BARRAY_REALLOC(
		header, sizeof(barray_header_t) + elem_size * new_capacity, ctx
	);
	new_header->capacity = new_capacity;
	return new_header->elems;
}

void*
barray__do_resize(void* array, size_t new_len, size_t elem_size, void* ctx) {
	barray_header_t* header = barray__header_of(array);
	size_t current_capacity = header != NULL ? header->capacity : 0;

	size_t old_len = header != NULL ? header->len : 0;
	if (new_len == old_len) {
		return array;
	} else if (new_len <= current_capacity) {
		header->len = new_len;
	} else {
		barray_header_t* new_header = BARRAY_REALLOC(
			header, sizeof(barray_header_t) + elem_size * new_len, ctx
		);
		new_header->capacity = new_len;
		new_header->len = new_len;
		header = new_header;
	}

	if (new_len > old_len) {
		// Zero new elements
		memset(
			header->elems + old_len * elem_size,
			0,
			(new_len - old_len) * elem_size
		);
	}

	return header->elems;
}

void
barray__do_pop(void* array) {
	barray_header_t* header = barray__header_of(array);
	header->len -= 1;
}

#endif
