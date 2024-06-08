#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

typedef intptr_t mem_layout_t;

static inline intptr_t
mem_layout_align_ptr(intptr_t ptr, size_t alignment) {
	return (((intptr_t)ptr + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

static inline ptrdiff_t
mem_layout_reserve(
	mem_layout_t* layout,
	size_t size,
	size_t alignment
) {
	intptr_t base = *layout;
	intptr_t ptr = mem_layout_align_ptr(base, alignment);
	*layout = ptr + (intptr_t)size;
	return (ptrdiff_t)ptr;
}

static inline size_t
mem_layout_size(mem_layout_t* layout) {
	return (size_t)*layout;
}

static inline void*
mem_layout_locate(void* mem, ptrdiff_t offset) {
	return (void*)((intptr_t)mem + offset);
}

#endif
