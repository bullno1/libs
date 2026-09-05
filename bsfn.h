#ifndef BSFN_H
#define BSFN_H

/**
 * @file
 * @brief Stable function pointers for hot-reloadable modules.
 *
 * When a module is hot reloaded, every callback pointer that was handed to a
 * long-lived system (e.g @ref bresmon_watch) dangles into the unloaded module.
 * @ref BSFN solves this by returning a *stable* pointer instead: a small JIT stub,
 * allocated outside of any module, that jumps to the wrapped function.
 * After each (re)load, a single call to @ref bsfn_reload repoints every stub
 * in the calling module at the new function addresses.
 *
 * In **exactly one** source file of **every module** (host executable and each
 * reloadable module) that uses this library, define `BSFN_IMPLEMENTATION`
 * before including bsfn.h.
 *
 * The host owns the context:
 *
 * @snippet samples/bsfn.c bsfn_host
 *
 * A module only has to wrap its callbacks with @ref BSFN and call
 * @ref bsfn_reload once per (re)load:
 *
 * @snippet samples/bsfn.c bsfn_module
 *
 * Define `BSFN_NO_RELOAD` (typically in release builds where modules are
 * statically linked) to turn @ref BSFN into a zero-cost passthrough and every
 * function into a no-op.
 *
 * @remarks
 *   Hot reloading is only supported on x86_64 unix with GCC or Clang.
 *   MSVC is always built in passthrough mode: @ref BSFN relies on a statement
 *   expression to register a function from within an expression, which MSVC
 *   does not have.
 *   On other unsupported targets, define `BSFN_NO_RELOAD` explicitly.
 */

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <stddef.h>

#ifndef BSFN_API
#define BSFN_API
#endif

/** @cond */
#ifndef BSFN_ASSERT
#include <assert.h>
#define BSFN_ASSERT assert
#endif
/** @endcond */

#ifndef DOXYGEN

typedef void (*bsfn_fn_t)(void);

typedef struct bsfn_ctx_s bsfn_ctx_t;

typedef struct {
	const char* name;
	bsfn_fn_t fn;
	bsfn_fn_t* slot;
} bsfn_reg_t;

#endif

// MSVC has no statement expression, which BSFN needs to declare its
// registration record at the call site, so it only gets the passthrough
#if defined(_MSC_VER) && !defined(BSFN_NO_RELOAD)
#	define BSFN_NO_RELOAD
#endif

#ifndef BSFN_NO_RELOAD

#if !defined(DOXYGEN) \
	&& !((defined(__GNUC__) || defined(__clang__)) \
		&& defined(__x86_64__) \
		&& defined(__unix__) \
		&& !defined(__APPLE__))
#error "bsfn only supports x86_64 unix with GCC/Clang; define BSFN_NO_RELOAD to make BSFN a passthrough"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a stub registry.
 *
 * This must be called by the host (the non-reloadable part of the program),
 * once, before any module is loaded.
 * The context is typically passed to modules alongside other host state.
 *
 * @param memctx Memory context.
 *   This will be passed to `BSFN_REALLOC`.
 *
 * @return The registry.
 */
BSFN_API bsfn_ctx_t*
bsfn_ctx_create(void* memctx);

/**
 * @brief Destroy the registry.
 *
 * All stable pointers created from it become invalid.
 *
 * @see bsfn_ctx_create
 */
BSFN_API void
bsfn_ctx_destroy(bsfn_ctx_t* ctx);

#ifndef DOXYGEN

BSFN_API void
bsfn__reload(bsfn_ctx_t* ctx, const bsfn_reg_t* begin, const bsfn_reg_t* end);

BSFN_API void
bsfn__unload(bsfn_ctx_t* ctx, const bsfn_reg_t* begin, const bsfn_reg_t* end);

#endif

#ifdef __cplusplus
}
#endif

/** @cond */
#define BSFN__LIST_BEGIN \
	__extension__({ \
		extern const bsfn_reg_t __start_bsfn__regs[] __attribute__((weak)); \
		__start_bsfn__regs; \
	})
#define BSFN__LIST_END \
	__extension__({ \
		extern const bsfn_reg_t __stop_bsfn__regs[] __attribute__((weak)); \
		__stop_bsfn__regs; \
	})

static inline bsfn_fn_t
bsfn__load_slot(bsfn_fn_t* slot) {
	BSFN_ASSERT(*slot != NULL && "bsfn_reload was not called in this module");
	return *slot;
}
/** @endcond */

/**
 * @brief Repoint all stubs of the calling module at its current functions.
 *
 * This must be called every time the module is loaded, **including the first
 * time**, before any @ref BSFN expression in the module is evaluated.
 *
 * @param ctx The registry, received from the host.
 */
static inline void
bsfn_reload(bsfn_ctx_t* ctx) {
	bsfn__reload(ctx, BSFN__LIST_BEGIN, BSFN__LIST_END);
}

/**
 * @brief Detach all stubs of the calling module.
 *
 * This is optional and only needed when a module is unloaded *for good*.
 *
 * @param ctx The registry, received from the host.
 */
static inline void
bsfn_unload(bsfn_ctx_t* ctx) {
	bsfn__unload(ctx, BSFN__LIST_BEGIN, BSFN__LIST_END);
}

/**
 * @brief Wrap a function, yielding a stable pointer to it.
 *
 * This is an expression and has the same type as `&FN`.
 * The returned pointer never changes across reloads of the image containing
 * `FN`; use it anywhere a callback would outlive the current image.
 *
 * `FN` must be a function, not a function pointer variable.
 * @ref bsfn_reload must have been called in the current image beforehand.
 */
#define BSFN(FN) \
	((__typeof__(&(FN)))(__extension__({ \
		static bsfn_fn_t bsfn__slot = NULL; \
		/* aligned() stops the compiler from over-aligning the record and */ \
		/* thus inserting padding between entries of the section. */ \
		__attribute__((retain, used, section("bsfn__regs"), aligned(sizeof(void*)))) \
		static const bsfn_reg_t bsfn__reg = { \
			.name = __FILE__ ":" #FN, \
			.fn = (bsfn_fn_t)&(FN), \
			.slot = &bsfn__slot, \
		}; \
		bsfn__load_slot(&bsfn__slot); \
	})))

#else /* BSFN_NO_RELOAD */

#define BSFN(FN) (FN)

static inline bsfn_ctx_t*
bsfn_ctx_create(void* memctx) {
	(void)memctx;
	return NULL;
}

static inline void
bsfn_ctx_destroy(bsfn_ctx_t* ctx) {
	(void)ctx;
}

static inline void
bsfn_reload(bsfn_ctx_t* ctx) {
	(void)ctx;
}

static inline void
bsfn_unload(bsfn_ctx_t* ctx) {
	(void)ctx;
}

#endif /* BSFN_NO_RELOAD */

#endif /* BSFN_H */

#if defined(BLIB_IMPLEMENTATION) && !defined(BSFN_IMPLEMENTATION)
#define BSFN_IMPLEMENTATION
#endif

#if defined(BSFN_IMPLEMENTATION) && !defined(BSFN_NO_RELOAD) && !defined(BSFN_IMPLEMENTED)
#define BSFN_IMPLEMENTED

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef BSFN_REALLOC
#	ifdef BLIB_REALLOC
#		define BSFN_REALLOC BLIB_REALLOC
#	else
#		define BSFN_REALLOC(ptr, size, ctx) bsfn__libc_realloc(ptr, size, ctx)
#		define BSFN_USE_LIBC
#	endif
#endif

#ifdef BSFN_USE_LIBC

#include <stdlib.h>

static inline void*
bsfn__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

// A stub occupies BSFN_STUB_SIZE bytes in the executable half of a chunk and
// reads its jump target from the slot placed exactly page_size bytes after it
// in the writable half.  Patching a stub is thus a plain data store: the code
// itself never changes once the chunk is made executable.
#define BSFN__STUB_SIZE 16

typedef struct {
	char* name;
	bsfn_fn_t code;
	bsfn_fn_t* target;
} bsfn__rec_t;

struct bsfn_ctx_s {
	void* memctx;
	size_t page_size;
	size_t stubs_per_chunk;

	uint8_t** chunks;
	size_t num_chunks;
	size_t cap_chunks;
	size_t num_stubs;

	bsfn__rec_t* recs;
	size_t num_recs;
	size_t cap_recs;

	bsfn_fn_t trap;
};

static bool
bsfn__chunk_add(bsfn_ctx_t* ctx) {
	size_t page_size = ctx->page_size;
	uint8_t* base = mmap(
		NULL, page_size * 2,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0
	);
	if (base == MAP_FAILED) { return false; }

	// jmp qword ptr [rip + page_size - 6], padded with int3
	uint32_t disp32 = (uint32_t)(page_size - 6);
	for (size_t i = 0; i < ctx->stubs_per_chunk; ++i) {
		uint8_t* stub = base + i * BSFN__STUB_SIZE;
		memset(stub, 0xcc, BSFN__STUB_SIZE);
		stub[0] = 0xff;
		stub[1] = 0x25;
		memcpy(&stub[2], &disp32, sizeof(disp32));
	}
	if (ctx->num_chunks == 0) {
		// The very first stub is the trap: ud2
		base[0] = 0x0f;
		base[1] = 0x0b;
	}
	if (mprotect(base, page_size, PROT_READ | PROT_EXEC) != 0) {
		munmap(base, page_size * 2);
		return false;
	}

	if (ctx->num_chunks >= ctx->cap_chunks) {
		size_t new_cap = ctx->cap_chunks > 0 ? ctx->cap_chunks * 2 : 4;
		uint8_t** chunks = BSFN_REALLOC(
			ctx->chunks, new_cap * sizeof(uint8_t*), ctx->memctx
		);
		if (chunks == NULL) {
			munmap(base, page_size * 2);
			return false;
		}
		ctx->chunks = chunks;
		ctx->cap_chunks = new_cap;
	}
	ctx->chunks[ctx->num_chunks++] = base;
	return true;
}

static bsfn_fn_t
bsfn__stub_alloc(bsfn_ctx_t* ctx, bsfn_fn_t** target_out) {
	size_t index = ctx->num_stubs;
	size_t chunk_index = index / ctx->stubs_per_chunk;
	size_t stub_index = index % ctx->stubs_per_chunk;
	if (chunk_index >= ctx->num_chunks && !bsfn__chunk_add(ctx)) {
		return NULL;
	}

	uint8_t* base = ctx->chunks[chunk_index];
	ctx->num_stubs += 1;
	*target_out = (bsfn_fn_t*)(void*)(
		base + ctx->page_size + stub_index * BSFN__STUB_SIZE
	);
	return (bsfn_fn_t)(uintptr_t)(base + stub_index * BSFN__STUB_SIZE);
}

static bsfn__rec_t*
bsfn__find(bsfn_ctx_t* ctx, const char* name) {
	for (size_t i = 0; i < ctx->num_recs; ++i) {
		if (strcmp(ctx->recs[i].name, name) == 0) {
			return &ctx->recs[i];
		}
	}
	return NULL;
}

static bsfn__rec_t*
bsfn__add(bsfn_ctx_t* ctx, const char* name) {
	if (ctx->num_recs >= ctx->cap_recs) {
		size_t new_cap = ctx->cap_recs > 0 ? ctx->cap_recs * 2 : 8;
		bsfn__rec_t* recs = BSFN_REALLOC(
			ctx->recs, new_cap * sizeof(bsfn__rec_t), ctx->memctx
		);
		if (recs == NULL) { return NULL; }
		ctx->recs = recs;
		ctx->cap_recs = new_cap;
	}

	// The name must be copied: the string literal dies with the module
	size_t name_len = strlen(name);
	char* name_copy = BSFN_REALLOC(NULL, name_len + 1, ctx->memctx);
	if (name_copy == NULL) { return NULL; }
	memcpy(name_copy, name, name_len + 1);

	bsfn_fn_t* target = NULL;
	bsfn_fn_t code = bsfn__stub_alloc(ctx, &target);
	if (code == NULL) {
		BSFN_REALLOC(name_copy, 0, ctx->memctx);
		return NULL;
	}

	bsfn__rec_t* rec = &ctx->recs[ctx->num_recs++];
	*rec = (bsfn__rec_t){
		.name = name_copy,
		.code = code,
		.target = target,
	};
	return rec;
}

BSFN_API bsfn_ctx_t*
bsfn_ctx_create(void* memctx) {
	long page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size <= 0) { return NULL; }

	bsfn_ctx_t* ctx = BSFN_REALLOC(NULL, sizeof(bsfn_ctx_t), memctx);
	if (ctx == NULL) { return NULL; }
	*ctx = (bsfn_ctx_t){
		.memctx = memctx,
		.page_size = (size_t)page_size,
		.stubs_per_chunk = (size_t)page_size / BSFN__STUB_SIZE,
	};

	// Reserve stub 0 as the trap
	bsfn_fn_t* target = NULL;
	ctx->trap = bsfn__stub_alloc(ctx, &target);
	if (ctx->trap == NULL) {
		BSFN_REALLOC(ctx, 0, memctx);
		return NULL;
	}

	return ctx;
}

BSFN_API void
bsfn_ctx_destroy(bsfn_ctx_t* ctx) {
	for (size_t i = 0; i < ctx->num_recs; ++i) {
		BSFN_REALLOC(ctx->recs[i].name, 0, ctx->memctx);
	}
	BSFN_REALLOC(ctx->recs, 0, ctx->memctx);
	for (size_t i = 0; i < ctx->num_chunks; ++i) {
		munmap(ctx->chunks[i], ctx->page_size * 2);
	}
	BSFN_REALLOC(ctx->chunks, 0, ctx->memctx);
	BSFN_REALLOC(ctx, 0, ctx->memctx);
}

BSFN_API void
bsfn__reload(bsfn_ctx_t* ctx, const bsfn_reg_t* begin, const bsfn_reg_t* end) {
	for (const bsfn_reg_t* reg = begin; reg != end; ++reg) {
		bsfn__rec_t* rec = bsfn__find(ctx, reg->name);
		if (rec == NULL) {
			rec = bsfn__add(ctx, reg->name);
			BSFN_ASSERT(rec != NULL && "bsfn: out of memory");
			if (rec == NULL) { continue; }
		}
		*(bsfn_fn_t volatile*)rec->target = reg->fn;
		*reg->slot = rec->code;
	}
}

BSFN_API void
bsfn__unload(bsfn_ctx_t* ctx, const bsfn_reg_t* begin, const bsfn_reg_t* end) {
	for (const bsfn_reg_t* reg = begin; reg != end; ++reg) {
		bsfn__rec_t* rec = bsfn__find(ctx, reg->name);
		if (rec != NULL) {
			*(bsfn_fn_t volatile*)rec->target = ctx->trap;
		}
	}
}

#endif
