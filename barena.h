#ifndef BARENA_H
#define BARENA_H

/**
 * @file
 * @brief Arena allocator backed by OS pages.
 *
 * Everyone knows [what an arena is](https://en.wikipedia.org/wiki/Nowe_Ateny#Legacy).
 *
 * Memory is obtained in chunks directly from the OS (`mmap` on Linux,
 * `VirtualAlloc` on Windows) through a @ref barena_pool_t.
 * An arena bumps a pointer through its current chunk and takes a new chunk
 * from the pool when it runs out.
 * A request larger than the chunk size gets a dedicated chunk.
 *
 * Chunks released by @ref barena_restore or @ref barena_reset go back to
 * the pool to be reused by any arena sharing it.
 * A single pool can thus serve many short-lived arenas (e.g: one per frame
 * or per task) without constantly returning memory to the OS.
 *
 * A plain bump allocator hides memory errors since the whole chunk is a
 * single valid allocation as far as any tool is concerned.
 * When AddressSanitizer is detected, chunks are poisoned and only the exact
 * bytes handed out are unpoisoned, with a redzone around every block.
 * Overflowing an allocation or touching memory released by
 * @ref barena_restore is then reported just like a heap error.
 * Define `BARENA_ASAN` to 0 to opt out or to 1 to force it on.
 *
 * In **exactly one** source file, define `BARENA_IMPLEMENTATION` before including barena.h.
 */

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <stddef.h>

#ifndef BARENA_API
#define BARENA_API
#endif

/*! A chunk of memory obtained from the OS, treat as opaque */
typedef struct barena_chunk_s barena_chunk_t;

/**
 * @brief A pool of chunks shared between arenas.
 *
 * It must be initialized with @ref barena_pool_init and outlive every arena
 * created from it.
 */
typedef struct barena_pool_s {
	/*! Size of each chunk, rounded up to the OS page size */
	size_t chunk_size;
	/*! The OS page size */
	size_t os_page_size;
	/*! Chunks released by arenas, waiting to be reused */
	barena_chunk_t* free_chunks;
} barena_pool_t;

/*! An arena */
typedef struct barena_s {
	/*! The chunk allocations are currently served from, NULL when empty */
	barena_chunk_t* current_chunk;
	/*! The pool chunks are taken from */
	barena_pool_t* pool;
} barena_t;

/**
 * @brief A position in an arena.
 *
 * @see barena_snapshot
 * @see barena_restore
 */
typedef char* barena_snapshot_t;

/**
 * @brief Initialize a pool.
 *
 * @param pool The pool.
 * @param chunk_size Size of each chunk.
 *   It will be rounded up to the OS page size.
 */
BARENA_API void
barena_pool_init(barena_pool_t* pool, size_t chunk_size);

/**
 * @brief Return all free chunks of a pool to the OS.
 *
 * Chunks still held by arenas are not affected.
 * Reset those arenas with @ref barena_reset first to release everything.
 */
BARENA_API void
barena_pool_cleanup(barena_pool_t* pool);

/**
 * @brief Initialize an empty arena.
 *
 * @param arena The arena.
 * @param pool The pool to take chunks from.
 */
BARENA_API void
barena_init(barena_t* arena, barena_pool_t* pool);

/**
 * @brief Allocate memory suitably aligned for any type.
 *
 * @param arena The arena.
 * @param size Number of bytes.
 *
 * @return The allocated memory or NULL if `size` is 0.
 *
 * @see barena_memalign
 */
BARENA_API void*
barena_malloc(barena_t* arena, size_t size);

/**
 * @brief Allocate memory with an explicit alignment.
 *
 * @param arena The arena.
 * @param size Number of bytes.
 * @param alignment The alignment, must be a power of 2.
 *
 * @return The allocated memory or NULL if `size` is 0.
 */
BARENA_API void*
barena_memalign(barena_t* arena, size_t size, size_t alignment);

/**
 * @brief Capture the current position of an arena.
 *
 * Everything allocated after this point can be released at once with
 * @ref barena_restore.
 *
 * @return The snapshot, NULL for an empty arena.
 */
BARENA_API barena_snapshot_t
barena_snapshot(barena_t* arena);

/**
 * @brief Rewind an arena to a snapshot.
 *
 * All memory allocated since the snapshot is released and chunks that are no
 * longer needed are returned to the pool.
 *
 * @param arena The arena.
 * @param snapshot A snapshot previously taken from the same arena.
 */
BARENA_API void
barena_restore(barena_t* arena, barena_snapshot_t snapshot);

/**
 * @brief Release all memory allocated from an arena.
 *
 * Its chunks are returned to the pool.
 * The arena can be used again immediately.
 */
BARENA_API void
barena_reset(barena_t* arena);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BARENA_IMPLEMENTATION)
#define BARENA_IMPLEMENTATION
#endif

#ifdef BARENA_IMPLEMENTATION

#include <stdint.h>

#ifndef BARENA_ASAN
#	if defined(__SANITIZE_ADDRESS__)  // gcc, msvc
#		define BARENA_ASAN 1
#	elif defined(__has_feature)  // clang
#		if __has_feature(address_sanitizer)
#			define BARENA_ASAN 1
#		else
#			define BARENA_ASAN 0
#		endif
#	else
#		define BARENA_ASAN 0
#	endif
#endif

#if BARENA_ASAN

#include <sanitizer/asan_interface.h>

#define BARENA_POISON(ptr, size) __asan_poison_memory_region((ptr), (size))
#define BARENA_UNPOISON(ptr, size) __asan_unpoison_memory_region((ptr), (size))
// The shadow map has one byte per 8 bytes of memory so a block must start on
// an 8 byte boundary, otherwise unpoisoning it would also unpoison the tail of
// whatever comes before
#define BARENA_MIN_ALIGNMENT 8
// Poisoned gap kept before and after every block so that overrunning one is
// caught instead of silently landing in its neighbour
#define BARENA_REDZONE_SIZE 8

#else

#define BARENA_POISON(ptr, size) ((void)(ptr), (void)(size))
#define BARENA_UNPOISON(ptr, size) ((void)(ptr), (void)(size))
#define BARENA_MIN_ALIGNMENT 1
#define BARENA_REDZONE_SIZE 0

#endif

#ifdef _MSC_VER
#	define MAX_ALIGN_TYPE double
#else
#	define MAX_ALIGN_TYPE max_align_t
#endif

static inline size_t
barena_os_page_size(void);

static inline void*
barena_os_page_alloc(size_t size);

static inline void
barena_os_page_free(void* ptr, size_t size);

static inline intptr_t
barena_align_ptr(intptr_t ptr, size_t alignment) {
	return (((intptr_t)ptr + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

struct barena_chunk_s {
	barena_chunk_t* next;
	char* bump_ptr;
	char* end;
	char begin[];
};

void
barena_pool_init(barena_pool_t* pool, size_t chunk_size) {
	size_t page_size = barena_os_page_size();
	chunk_size = (size_t)barena_align_ptr((intptr_t)chunk_size, page_size);
	*pool = (barena_pool_t){
		.os_page_size = page_size,
		.chunk_size = chunk_size,
	};
}

void
barena_pool_cleanup(barena_pool_t* pool) {
	for (
		barena_chunk_t* chunk_itr = pool->free_chunks;
		chunk_itr != NULL;
	) {
		barena_chunk_t* next = chunk_itr->next;
		size_t chunk_size = (size_t)(chunk_itr->end - (char*)chunk_itr);
		// Whatever reuses this memory next must not find our poison in it
		BARENA_UNPOISON(chunk_itr, chunk_size);
		barena_os_page_free(chunk_itr, chunk_size);
		chunk_itr = next;
	}

	pool->free_chunks = NULL;
}

void
barena_init(barena_t* arena, barena_pool_t* pool) {
	*arena = (barena_t){
		.pool = pool,
	};
}

void*
barena_malloc(barena_t* arena, size_t size) {
	return barena_memalign(arena, size, _Alignof(MAX_ALIGN_TYPE));
}

static inline void*
barena_alloc_from_chunk(barena_chunk_t* chunk, size_t size, size_t alignment) {
	size_t space_available = chunk != NULL ? chunk->end - chunk->begin : 0;
	if (space_available < size) { return NULL; }

	intptr_t result = barena_align_ptr((intptr_t)chunk->bump_ptr, alignment);
	intptr_t new_bump_ptr = result + (ptrdiff_t)size + BARENA_REDZONE_SIZE;
	if (new_bump_ptr > (intptr_t)chunk->end) { return NULL; }

	chunk->bump_ptr = (char*)new_bump_ptr;
	BARENA_UNPOISON((void*)result, size);
	return (void*)result;
}

void*
barena_memalign(barena_t* arena, size_t size, size_t alignment) {
	if (size == 0) { return NULL; }
	if (alignment < BARENA_MIN_ALIGNMENT) { alignment = BARENA_MIN_ALIGNMENT; }

	barena_chunk_t* current_chunk = arena->current_chunk;
	void* result = barena_alloc_from_chunk(current_chunk, size, alignment);
	if (result != NULL) { return result; }

	// New chunk needed
	barena_pool_t* pool = arena->pool;
	size_t chunk_size = pool->chunk_size;
	size_t required_size = (size_t)barena_align_ptr(
		// A redzone on each side of the block
		(intptr_t)(sizeof(barena_chunk_t) + BARENA_REDZONE_SIZE + size + BARENA_REDZONE_SIZE),
		pool->os_page_size
	);
	size_t alloc_size = chunk_size >= required_size ? chunk_size : required_size;

	barena_chunk_t* new_chunk;
	if (
		pool->free_chunks != NULL
		&& (size_t)(pool->free_chunks->end - (char*)pool->free_chunks) >= alloc_size
	) {
		new_chunk = pool->free_chunks;
		pool->free_chunks = new_chunk->next;
	} else {
		new_chunk = barena_os_page_alloc(alloc_size);
		new_chunk->end = (char*)new_chunk + alloc_size;
	}

	// The leading redzone guards the chunk header against an underrun of the
	// first block
	new_chunk->bump_ptr = new_chunk->begin + BARENA_REDZONE_SIZE;
	new_chunk->next = arena->current_chunk;
	arena->current_chunk = new_chunk;
	BARENA_POISON(new_chunk->begin, (size_t)(new_chunk->end - new_chunk->begin));

	return barena_alloc_from_chunk(new_chunk, size, alignment);
}

barena_snapshot_t
barena_snapshot(barena_t* arena) {
	return arena->current_chunk != NULL
		? arena->current_chunk->bump_ptr
		: NULL;
}

void
barena_restore(barena_t* arena, barena_snapshot_t snapshot) {
	barena_pool_t* pool = arena->pool;
	barena_chunk_t* itr = arena->current_chunk;
	while (
		itr != NULL
		&& !(itr->begin <= snapshot && snapshot <= itr->end)
	) {
		barena_chunk_t* next = itr->next;

		BARENA_POISON(itr->begin, (size_t)(itr->end - itr->begin));
		itr->next = pool->free_chunks;
		pool->free_chunks = itr;

		itr = next;
	}

	if (snapshot != NULL) {
		BARENA_POISON(snapshot, (size_t)(itr->end - snapshot));
		itr->bump_ptr = snapshot;
	}
	arena->current_chunk = itr;
}

void
barena_reset(barena_t* arena) {
	barena_restore(arena, NULL);
}

#if defined(__EMSCRIPTEN__)

#include <stdlib.h>

size_t
barena_os_page_size(void) {
	return (size_t)4096;
}

void*
barena_os_page_alloc(size_t size) {
	return malloc(size);
}

void
barena_os_page_free(void* ptr, size_t size) {
	(void)size;
	free(ptr);
}

#elif defined(__linux__) || defined(__COSMOPOLITAN__) || defined(__FreeBSD__)

#include <unistd.h>
#include <sys/mman.h>

size_t
barena_os_page_size(void) {
	return (size_t)sysconf(_SC_PAGE_SIZE);
}

void*
barena_os_page_alloc(size_t size) {
	return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void
barena_os_page_free(void* ptr, size_t size) {
	munmap(ptr, size);
}

#elif defined(_WIN32)

#ifndef WIN32_LEAND_AND_MEAN
#define WIN32_LEAND_AND_MEAN
#endif

#include <Windows.h>

size_t
barena_os_page_size(void) {
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
}

void*
barena_os_page_alloc(size_t size) {
	return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
}

void
barena_os_page_free(void* ptr, size_t size) {
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
}

#else
#	error "Unsupported platform"
#endif

#endif
