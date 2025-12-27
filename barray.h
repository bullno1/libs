#ifndef BARRAY_H
#define BARRAY_H

#include <stddef.h>

#ifndef BARRAY_API
#define BARRAY_API
#endif

#define barray(T) T*

#define barray_push(array, element, ctx) \
	do { \
		size_t barray__new_len; \
		(array) = barray__prepare_push((array), &barray__new_len, sizeof(element), ctx); \
		(array)[barray__new_len - 1] = element; \
	} while (0)

#define barray_reserve(array, new_capacity, ctx) \
	do { \
		(array) = barray__do_reserve((array), new_capacity, sizeof(*(array)), ctx); \
	} while (0)

#define barray_resize(array, new_len, ctx) \
	do { \
		(array) = barray__do_resize((array), new_len, sizeof(*(array)), ctx); \
	} while (0)

#define barray_pop(array) (barray__do_pop((array)), array[barray_len((array))])

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

BARRAY_API size_t
barray_len(void* array);

BARRAY_API size_t
barray_capacity(void* array);

BARRAY_API void
barray_free(void* array, void* ctx);

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
barray_free(void* array, void* ctx) {
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

	size_t old_len = header->len;
	if (new_len <= current_capacity) {
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
