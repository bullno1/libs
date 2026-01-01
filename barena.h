#ifndef BARENA_H
#define BARENA_H

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <stddef.h>

#ifndef BARENA_API
#define BARENA_API
#endif

typedef struct barena_chunk_s barena_chunk_t;

typedef struct barena_pool_s {
	size_t chunk_size;
	size_t os_page_size;
	barena_chunk_t* free_chunks;
} barena_pool_t;

typedef struct barena_s {
	barena_chunk_t* current_chunk;
	barena_pool_t* pool;
} barena_t;

typedef char* barena_snapshot_t;

BARENA_API void
barena_pool_init(barena_pool_t* pool, size_t chunk_size);

BARENA_API void
barena_pool_cleanup(barena_pool_t* pool);

BARENA_API void
barena_init(barena_t* arena, barena_pool_t* pool);

BARENA_API void*
barena_malloc(barena_t* arena, size_t size);

BARENA_API void*
barena_memalign(barena_t* arena, size_t size, size_t alignment);

BARENA_API barena_snapshot_t
barena_snapshot(barena_t* arena);

BARENA_API void
barena_restore(barena_t* arena, barena_snapshot_t snapshot);

BARENA_API void
barena_reset(barena_t* arena);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BARENA_IMPLEMENTATION)
#define BARENA_IMPLEMENTATION
#endif

#ifdef BARENA_IMPLEMENTATION

#include <stdint.h>

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
		barena_os_page_free(chunk_itr, chunk_itr->end - (char*)chunk_itr);
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
	intptr_t new_bump_ptr = result + (ptrdiff_t)size;
	if (new_bump_ptr > (intptr_t)chunk->end) { return NULL; }

	chunk->bump_ptr = (char*)new_bump_ptr;
	return (void*)result;
}

void*
barena_memalign(barena_t* arena, size_t size, size_t alignment) {
	if (size == 0) { return NULL; }

	barena_chunk_t* current_chunk = arena->current_chunk;
	void* result = barena_alloc_from_chunk(current_chunk, size, alignment);
	if (result != NULL) { return result; }

	// New chunk needed
	barena_pool_t* pool = arena->pool;
	size_t chunk_size = pool->chunk_size;
	size_t required_size = (size_t)barena_align_ptr(
		(intptr_t)(sizeof(barena_chunk_t) + size),
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

	new_chunk->bump_ptr = new_chunk->begin;
	new_chunk->next = arena->current_chunk;
	arena->current_chunk = new_chunk;

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

		itr->next = pool->free_chunks;
		pool->free_chunks = itr;

		itr = next;
	}

	if (snapshot != NULL) {
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
