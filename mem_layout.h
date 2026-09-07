#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

/**
 * @file
 * @brief Combine multiple allocations of a nested struct into one.
 *
 * Given a struct where its members have variable size such as this:
 *
 * ```c
 * typedef struct {
 *     int num_ints;
 *     int* ints;
 *     int num_floats;
 *     float* floats;
 * } var_struct;
 * ```
 *
 * mem_layout can be used to calculate the size of a single buffer that fits
 * the struct and all of its nested members, and to locate each member in it:
 *
 * 1. Start from a zero-initialized @ref mem_layout_t.
 * 2. Call mem_layout_reserve() once per member, in any order, and keep the
 *    returned offsets.
 * 3. Allocate mem_layout_size() bytes with any allocator.
 * 4. Turn each offset into a pointer with mem_layout_locate().
 *
 * @snippet tests/mem_layout/main.c mem_layout_example
 *
 * Everything is `static inline` so there is no implementation to define.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * @brief A layout being calculated.
 *
 * It holds the size of the buffer so far and must be initialized to 0.
 */
typedef intptr_t mem_layout_t;

/*! Round `ptr` up to a multiple of `alignment` (a power of 2) */
static inline intptr_t
mem_layout_align_ptr(intptr_t ptr, size_t alignment) {
	return (((intptr_t)ptr + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

/**
 * @brief Reserve space for a member.
 *
 * @param layout The layout.
 * @param size Size of the member.
 * @param alignment Alignment of the member, must be a power of 2.
 *
 * @return The offset of the member from the start of the buffer.
 *
 * @see mem_layout_locate
 */
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

/**
 * @brief Retrieve the size of the buffer needed for everything reserved so far.
 *
 * The buffer must be allocated with an alignment suitable for the first
 * member, e.g: with `malloc`.
 */
static inline size_t
mem_layout_size(mem_layout_t* layout) {
	return (size_t)*layout;
}

/**
 * @brief Locate a member in an allocated buffer.
 *
 * @param mem The buffer.
 * @param offset The offset returned by mem_layout_reserve().
 *
 * @return Pointer to the member.
 */
static inline void*
mem_layout_locate(void* mem, ptrdiff_t offset) {
	return (void*)((intptr_t)mem + offset);
}

#endif
