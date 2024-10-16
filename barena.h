#ifndef BARENA_H
#define BARENA_H

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <stddef.h>

#ifndef BARENA_SHARED
#	if defined(_WIN32) && !defined(__MINGW32__)
#		ifdef BARENA_IMPLEMENTATION
#			define BARENA_API __declspec(dllexport)
#		else
#			define BARENA_API __declspec(dllimport)
#		endif
#	else
#		ifdef BARENA_IMPLEMENTATION
#			define BARENA_API __attribute__((visibility("default")))
#		else
#			define BARENA_API
#		endif
#	endif
#else
#	define BARENA_API
#endif

typedef struct barena_s {
	char* begin;
	char* end;
	char* bump_ptr;
	char* bump_ptr_end;
	char* high_water_mark;
	size_t chunk_size;
} barena_t;

typedef void* barena_snapshot_t;

#ifdef __cplusplus
extern "C" {
#endif

BARENA_API void
barena_init(barena_t* arena, size_t max_size, size_t chunk_size);

BARENA_API void
barena_cleanup(barena_t* arena);

BARENA_API void*
barena_malloc(barena_t* arena, size_t size);

BARENA_API void*
barena_memalign(barena_t* arena, size_t alignment, size_t size);

BARENA_API barena_snapshot_t
barena_snapshot(barena_t* arena);

BARENA_API void
barena_restore(barena_t* arena, barena_snapshot_t snapshot);

#ifdef __cplusplus
}
#endif

#endif

#ifdef BARENA_IMPLEMENTATION

#include <stdint.h>
#include <assert.h>

#ifdef _MSC_VER
#	define MAX_ALIGN_TYPE double
#else
#	define MAX_ALIGN_TYPE max_align_t
#endif

static inline size_t
barena_os_page_size(void);

static inline void*
barena_os_reserve(size_t size);

static inline void
barena_os_release(void* ptr, size_t size);

static inline void
barena_os_commit(void* ptr, size_t size);

static inline void*
barena_align_ptr(void* ptr, size_t alignment);

void
barena_init(barena_t* arena, size_t max_size, size_t chunk_size) {
	size_t page_size = barena_os_page_size();
	max_size = (max_size / page_size) * page_size;
	chunk_size = (chunk_size / page_size) * page_size;

	char* base = barena_os_reserve(max_size);
	assert(base != NULL && "Could not reserve memory");

	arena->begin = base;
	arena->end = base + max_size;
	arena->high_water_mark = base;
	arena->chunk_size = chunk_size;
	arena->bump_ptr = base;
	arena->bump_ptr_end = base;
}

void
barena_cleanup(barena_t* arena) {
	barena_os_release(arena->begin, arena->end - arena->begin);
}

void*
barena_malloc(barena_t* arena, size_t size) {
	return barena_memalign(arena, _Alignof(MAX_ALIGN_TYPE), size);
}

void*
barena_memalign(barena_t* arena, size_t alignment, size_t size) {
	char* result = barena_align_ptr(arena->bump_ptr, alignment);
	char* next_bump_ptr = result + size;

	if (next_bump_ptr <= arena->end) {
		arena->bump_ptr = next_bump_ptr;
		char* high_water_mark = arena->high_water_mark;
		arena->high_water_mark = next_bump_ptr > high_water_mark ? next_bump_ptr : high_water_mark;

		char* bump_ptr_end = arena->bump_ptr_end;
		if (next_bump_ptr <= bump_ptr_end) {
			return result;
		} else {
			char* next_bump_ptr_end = barena_align_ptr(next_bump_ptr, arena->chunk_size);
			arena->bump_ptr_end = next_bump_ptr_end;
			barena_os_commit(bump_ptr_end, next_bump_ptr_end - bump_ptr_end);

			return result;
		}
	} else {
		return NULL;
	}
}

barena_snapshot_t
barena_snapshot(barena_t* arena) {
	return arena->bump_ptr;
}

void
barena_restore(barena_t* arena, barena_snapshot_t snapshot) {
	arena->bump_ptr = snapshot;
}

void*
barena_align_ptr(void* ptr, size_t alignment) {
	return (void*)(((intptr_t)ptr + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

#if defined(__linux__)

#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>

size_t
barena_os_page_size(void) {
	return (size_t)sysconf(_SC_PAGESIZE);
}

void*
barena_os_reserve(size_t size) {
	return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
}

void
barena_os_release(void* ptr, size_t size) {
	munmap(ptr, size);
}

void
barena_os_commit(void* ptr, size_t size) {
	int ret = mprotect(ptr, size, PROT_READ | PROT_WRITE);
	(void)ret;
	assert(ret == 0);
}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

size_t
barena_os_page_size(void) {
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
}

void*
barena_os_reserve(size_t size) {
	return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

void
barena_os_release(void* ptr, size_t size) {
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
}

void
barena_os_commit(void* ptr, size_t size) {
	void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	(void)result;
	assert(result != NULL);
}

#else
#	error "Unsupported platform"
#endif

#endif
