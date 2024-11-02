#ifndef BARRAY_H
#define BARRAY_H

#include <stddef.h>

#define barray(T) T*

#define barray_push(array, element, ctx) \
	do { \
		size_t new_len; \
		array = barray__prepare_push(array, &new_len, sizeof(element), ctx); \
		array[new_len - 1] = element; \
	} while (0)

#define barray_reserve(array, new_capacity, ctx) \
	do { \
		array = barray__do_reserve(array, new_capacity, sizeof(*array), ctx); \
	} while (0)

#define barray_resize(array, new_len, ctx) \
	do { \
		array = barray__do_resize(array, new_len, sizeof(*array), ctx); \
	} while (0)

#define barray_pop(array) array[barray_len(array) - 1], barray__do_pop(array)

typedef struct barray_header_s {
	size_t capacity;
	size_t len;
	_Alignas(max_align_t) char elems[];
} barray_header_t;

static inline barray_header_t*
barray__header_of(void* array);

static inline size_t
barray_len(void* array) {
	barray_header_t* header = barray__header_of(array);
	return header != NULL ? header->len : 0;
}

static inline size_t
barray_capacity(void* array) {
	barray_header_t* header = barray__header_of(array);
	return header != NULL ? header->capacity : 0;
}

// Private

#ifndef BARRAY_REALLOC
#include <stdlib.h>
#define BARRAY_REALLOC(ptr, size, ctx) barray__libc_realloc(ptr, size, ctx)

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

static inline barray_header_t*
barray__header_of(void* array) {
	if (array != NULL) {
		return (barray_header_t*)((char*)array - offsetof(barray_header_t, elems));
	} else {
		return NULL;
	}
}

static inline void*
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

static inline void*
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

static inline void*
barray__do_resize(void* array, size_t new_len, size_t elem_size, void* ctx) {
	barray_header_t* header = barray__header_of(array);
	size_t current_capacity = header != NULL ? header->capacity : 0;

	if (new_len <= current_capacity) {
		header->len = new_len;
		return array;
	} else {
		barray_header_t* new_header = BARRAY_REALLOC(
			header, sizeof(barray_header_t) + elem_size * new_len, ctx
		);
		new_header->capacity = new_len;
		new_header->len = new_len;
		return new_header->elems;
	}
}

static inline void
barray__do_pop(void* array) {
	barray_header_t* header = barray__header_of(array);
	header->len -= 1;
}

#endif
