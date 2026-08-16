#ifndef BCO_H
#define BCO_H

/**
 * @file
 * @brief Coroutine using macro.
 *
 * In **exactly one** source file, define `BCO_IMPLEMENTATION` before including bco.h.
 *
 * To define a coroutine, use @ref bco and related macros.
 * Then it can be spawned with @ref bco_spawn.
 *
 * Within the coroutine, the following controls are available:
 *
 * * @ref bco_yield : Yield back to the caller
 * * @ref bco_call : Call into a subcoroutine
 * * @ref bco_return : Early return
 * * @ref bco_join : Wait for another coroutine
 *
 * To access arguments, use @ref bco_arg
 * To have local variables that get persisted between runs, use @ref bco_vars
 *
 * Checkout the rest of the documentation for other features such as @ref bco_copy, @ref bco_terminate...
 */

#include <stddef.h>

#ifndef BCO_API
#define BCO_API
#endif

#ifndef BCO_ASSERT
#include <assert.h>
#define BCO_ASSERT(COND, MSG) assert((COND) && (MSG))
#endif

#ifndef BCO_MAX_ALIGN
#define BCO_MAX_ALIGN 16
#endif

/**
 * Declare and implement a coroutine function
 *
 * Arguments should preferably be passed by value so they are copied into the
 * coroutine's private stack.
 * If there is any pointers, they should have their lifetime last for the entire
 * execution of the coroutine.
 *
 * The coroutine function has external linkage, so its name must be unique across
 * the whole program.
 * Use @ref bco_static for a coroutine that is private to one source file.
 *
 * @param NAME the name for the coroutine function
 * @param ... argument list with up to 6 arguments.
 *
 * Example:
 *
 * @snippet samples/bco.c bco
 *
 * @see bco_static
 */
#define bco(NAME, ...) \
	bco_decl(NAME, __VA_ARGS__); \
	bco_impl(NAME)

/**
 * Declare and implement a coroutine function with internal linkage
 *
 * Same as @ref bco but the coroutine function is private to the enclosing
 * source file, so its name will not collide with a coroutine of the same name
 * in another one.
 *
 * @param NAME the name for the coroutine function
 * @param ... argument list with up to 6 arguments.
 *
 * @see bco
 */
#define bco_static(NAME, ...) \
	bco_decl_static(NAME, __VA_ARGS__); \
	bco_impl(NAME)

/**
 * Forward declare a coroutine function without implementing it
 *
 * @param NAME the name for the coroutine function
 * @param ... argument list with up to 6 arguments.
 *
 * @see bco
 * @see bco_decl_static
 *
 * Example:
 *
 * @snippet samples/bco.c bco_decl
 *
 * @hideinitializer
 */
#define bco_decl(NAME, ...) bco__decl(extern, NAME, __VA_ARGS__)

/**
 * Forward declare a coroutine function with internal linkage
 *
 * @param NAME the name for the coroutine function
 * @param ... argument list with up to 6 arguments.
 *
 * The matching @ref bco_impl must appear in the same source file.
 * It does not need to repeat `static`, the definition inherits the linkage of
 * this declaration.
 *
 * @see bco_static
 * @see bco_decl
 *
 * @hideinitializer
 */
#define bco_decl_static(NAME, ...) bco__decl(static, NAME, __VA_ARGS__)

/**
 * Implement a coroutine function that was previously forward declared
 *
 * The linkage comes from the declaration, so this is used for both
 * @ref bco_decl and @ref bco_decl_static.
 *
 * @param NAME name of the coroutine function previously declared
 *
 * Example:
 *
 * @snippet samples/bco.c bco_impl
 *
 * @see bco_decl
 * @see bco_decl_static
 *
 * @hideinitializer
 */
#define bco_impl(NAME) bco__fn(NAME)

/**
 *  Access an argument from a coroutine's body
 *
 * @hideinitializer
 */
#define bco_arg(NAME) bco__args->NAME

/**
 * Access the associated user data from a coroutine's body
 *
 * This provides a retargetable environment for coroutine's code.
 *
 * @see bco_set_userdata
 * @see bco_get_userdata
 *
 * @hideinitializer
 */
#define bco_userdata bco_get_userdata(bco__coro)

/**
 * Declare coroutine variables
 *
 * They are persisted between runs.
 * They are zero-initialized when the coroutine body is first entered.
 *
 * Must be placed before @ref bco_begin and can only be used once.
 *
 * Example:
 *
 * @snippet samples/bco.c bco_vars
 *
 * @see bco_var
 *
 * @hideinitializer
 */
#define bco_vars(...) \
	_Static_assert(bco__begin_declared == 0, "bco_vars must be placed *before* bco_begin"); \
	_Static_assert(bco__vars_declared == 0, "bco_vars can only be used once"); \
	enum { bco__vars_declared = 1 }; \
	struct bco__vars_t { __VA_ARGS__ }* bco__vars = bco__alloc( \
		bco__coro, \
		sizeof(struct bco__vars_t), \
		_Alignof(struct bco__vars_t) \
	); \
	_Static_assert( \
		_Alignof(struct bco__vars_t) <= _Alignof(bco_align_t), \
		"Coroutine vars contain member(s) with alignment requirement above BCO_MAX_ALIGN" \
	); \
	(void)bco__vars;

/**
 * Access a coroutine variable
 *
 * @see bco_vars
 *
 * @hideinitializer
 */
#define bco_var(NAME) bco__vars->NAME

/**
 * Mark the beginning of a coroutine body
 *
 * Must appear after @ref bco_vars, can only be used once, and must be matched by
 * a @ref bco_end.
 *
 * @see bco_end
 *
 * @hideinitializer
 */
#define bco_begin \
	_Static_assert(bco__begin_declared == 0, "bco_begin can only be used once"); \
	enum { bco__begin_declared = 1 }; \
	switch (bco__on_resume(bco__coro)) { \
		case 0: \
			bco__begin_constant_cond \
			if (bco__vars_declared) { bco__zero_vars(bco__coro); } \
			bco__end_constant_cond

/**
 * Mark the end of a coroutine body
 *
 * Any code after this is the cleanup section.
 *
 * As long as the coroutine has been started with @ref bco_resume, it will always
 * be run regardless of whether the coroutine runs to completion, returns early
 * with @ref bco_return or is forcefully terminated with @ref bco_terminate.
 *
 * Example:
 *
 * @snippet samples/bco.c bco_end
 *
 * @see bco_terminate
 * @see bco_return
 *
 * @hideinitializer
 */
#define bco_end \
	default: { \
		_Static_assert(bco__begin_declared == 1, "bco_end must be placed *after* bco_begin"); \
		_Static_assert(bco__end_declared == 0, "bco_end can only be used once"); \
		goto bco__cleanup; \
		bco__cleanup: bco__on_terminate(bco__coro); \
	} /* default */ \
	} /* switch */ \
	enum { bco__end_declared = 1 };

/**
 * Yield the currently running coroutine
 *
 * Only valid between @ref bco_begin and @ref bco_end.
 *
 * @hideinitializer
 */
#define bco_yield() \
	do { \
		_Static_assert(bco__begin_declared == 1 && bco__end_declared == 0, "bco_yield can only be used *between* bco_begin and bco_end"); \
		bco__on_yield(bco__coro, __LINE__); \
		return; \
		case __LINE__:; \
	} while (0)

/**
 * Spawn a coroutine
 *
 * This is the entry point for starting a coroutine.
 *
 * `CORO` must point at storage that is at least @ref bco_mem_size bytes and
 * aligned as @ref bco_align_t.
 * The coroutine is left in @ref BCO_SUSPENDED, ready for @ref bco_resume.
 *
 * @param CORO the coroutine handle
 * @param NAME name of the entry function
 * @param ... arguments to pass to the function
 *
 * Example:
 *
 * @snippet samples/bco.c bco_spawn
 *
 * @hideinitializer
 */
#define bco_spawn(CORO, NAME, ...) \
	do { \
		bco__spawn( \
			CORO, \
			bco__concat(bco__wrapper_, NAME), \
			sizeof(bco__arg_type(NAME)), \
			_Alignof(bco__arg_type(NAME)), \
			&(bco__arg_type(NAME)){ __VA_ARGS__ } \
		); \
	} while (0)

/**
 * Spawn a subcoroutine from within a coroutine and transfer control to it
 *
 * @param NAME name of the entry function
 * @param ... arguments to pass to the function
 *
 * @hideinitializer
 */
#define bco_call(NAME, ...) \
	do { \
		_Static_assert(bco__begin_declared == 1 && bco__end_declared == 0, "bco_call can only be used *between* bco_begin and bco_end"); \
		bco_spawn(bco__alloc_subcoro(bco__coro), NAME, __VA_ARGS__); \
		bco_set_userdata(bco__subcoro(bco__coro), bco_get_userdata(bco__coro)); \
		bco_join(bco__subcoro(bco__coro)); \
		bco__free_subcoro(bco__coro); \
	} while (0)

/**
 * Wait for another coroutine to finish
 *
 * @param CORO the coroutine to wait for
 *
 * @hideinitializer
 */
#define bco_join(CORO) \
	do { \
		_Static_assert(bco__begin_declared == 1 && bco__end_declared == 0, "bco_join can only be used *between* bco_begin and bco_end"); \
		while (bco_resume(CORO) != BCO_TERMINATED) { bco_yield(); } \
	} while (0)

/**
 * Early return from the coroutine
 *
 * Control jumps straight to the cleanup section after @ref bco_end.
 * The coroutine ends up in @ref BCO_TERMINATED.
 *
 * Only valid between @ref bco_begin and @ref bco_end.
 *
 * This is the only correct way to leave a coroutine body early.
 *
 * @see bco_end
 *
 * @hideinitializer
 */
#define bco_return() \
	do { \
		_Static_assert(bco__begin_declared == 1 && bco__end_declared == 0, "bco_return can only be used *between* bco_begin and bco_end"); \
		goto bco__cleanup; \
	} while (0)

/**
 * A type that has the same alignment as @ref bco_t
 *
 * This can be used to stack-allocate a coroutine's storage.
 */
typedef struct { _Alignas(BCO_MAX_ALIGN) char bco__dummy; } bco_align_t;

/// The opaque type for a coroutine's storage
typedef struct bco_s bco_t;

/// Coroutine's status
typedef enum {
	/// The coroutine is running
	BCO_RUNNING,
	/// The coroutine is suspended
	BCO_SUSPENDED,
	/// The coroutine is terminated
	BCO_TERMINATED,
} bco_status_t;

/**
 * Required memory for a coroutine
 *
 * This can be used to heap-allocate a coroutine's storage.
 *
 * @param stack_size
 * @return required memory, in bytes
 */
BCO_API size_t
bco_mem_size(size_t stack_size);

/**
 * Resume a coroutine.
 *
 * @param coro the coroutine to resume
 * @return the coroutine's status
 */
BCO_API bco_status_t
bco_resume(bco_t* coro);

/**
 * Force terminate a coroutine
 *
 * The coroutine and all of its subcoroutine (from @ref bco_call) will
 * be forcefully terminated.
 *
 * Cleanup code after @ref bco_end will be run if the coroutine has started.
 *
 * @param coro the coroutine to terminate
 *
 * @see bco_end
 */
BCO_API void
bco_terminate(bco_t* coro);

/// Get the status of a coroutine
BCO_API bco_status_t
bco_status(bco_t* coro);

/**
 * Make a copy of a coroutine
 *
 * The coroutine stack's and its execution state is copied over.
 * However, this is a shallow copy.
 * Any pointer the coroutine has to external resources will remain shared in the new copy.
 * Making deep copy of resources is beyond the scope of this library.
 *
 * A way to achieve deep copy is to avoid pointers and use handles for all external resources.
 * Resolve all handles through a table stored in a userdata set by @ref bco_set_userdata.
 * Upon copying, this table can also be cloned and given to the new coroutine.
 *
 * For the copy to be safe, the destination must be at least as big as the source.
 *
 * @param dst destination coroutine to copy to.
 * @param src source coroutine to copy from
 *
 * @see bco_set_userdata
 * @see bco_userdata
 */
BCO_API void
bco_copy(bco_t* dst, bco_t* src);

/**
 * Set a userdata associated with the coroutine.
 *
 * From within the coroutine, this can be accessed with @ref bco_userdata.
 * This userdata is also inherited by any subcoroutine called through @ref bco_call.
 *
 * @param coro the coroutine
 * @param userdata pointer to arbitrary userdata
 *
 * @see bco_get_userdata
 * @see bco_userdata
 */
BCO_API void
bco_set_userdata(bco_t* coro, void* userdata);

/**
 * Retrieve the userdata associated with the coroutine.
 *
 * @param coro the coroutine
 * @return the associated userdata
 *
 * @see bco_set_userdata
 */
BCO_API void*
bco_get_userdata(bco_t* coro);

// Private

#ifndef DOXYGEN

#define bco__arg_type(NAME) bco__concat(bco__args_, NAME)

#define bco__fn(NAME) void NAME(bco_t* bco__coro, bco__arg_type(NAME)* bco__args)

#define bco__decl(LINKAGE, NAME, ...) \
	typedef struct bco__arg_type(NAME) bco__arg_type(NAME); \
	LINKAGE bco__fn(NAME); \
	static inline void bco__concat(bco__wrapper_, NAME)(bco_t* bco__coro, void* args) { \
		NAME(bco__coro, args); \
	} \
	struct bco__arg_type(NAME) { \
		bco__struct_fields(__VA_ARGS__ __VA_OPT__(,) -) \
	}; \
	_Static_assert( \
		_Alignof(bco__arg_type(NAME)) <= _Alignof(bco_align_t), \
		"Coroutine arguments contain member(s) with alignment requirement above BCO_MAX_ALIGN" \
	)

#define bco__struct_fields(...) bco__concat(bco__struct_field_, bco__count(__VA_ARGS__))(__VA_ARGS__)

#define bco__concat(LHS, RHS)  bco__concat2(LHS, RHS)
#define bco__concat2(LHS, RHS) LHS##RHS

#define bco__pick(_1, _2, _3, _4, _5, _6, _7, N, ...) N
#define bco__count(...) bco__pick(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, -)

#define bco__struct_field_1(ARG) char bco__dummy;
#define bco__struct_field_2(ARG, ...) ARG;
#define bco__struct_field_3(ARG, ...) ARG; bco__struct_field_2(__VA_ARGS__)
#define bco__struct_field_4(ARG, ...) ARG; bco__struct_field_3(__VA_ARGS__)
#define bco__struct_field_5(ARG, ...) ARG; bco__struct_field_4(__VA_ARGS__)
#define bco__struct_field_6(ARG, ...) ARG; bco__struct_field_5(__VA_ARGS__)
#define bco__struct_field_7(ARG, ...) ARG; bco__struct_field_6(__VA_ARGS__)

#ifdef _MSC_VER
#define bco__begin_constant_cond __pragma(warning(push)) __pragma(warning(disable: 4127))
#define bco__end_constant_cond __pragma(warning(pop))
#else
#define bco__begin_constant_cond
#define bco__end_constant_cond
#endif

typedef void (*bco_fn_t)(bco_t* coro, void* args);

BCO_API void
bco__spawn(bco_t* coro, bco_fn_t fn, size_t args_size, size_t args_alignment, void* args);

BCO_API void
bco__zero_vars(bco_t* coro);

BCO_API void*
bco__alloc(bco_t* coro, size_t size, size_t alignment);

BCO_API int
bco__on_resume(bco_t* coro);

BCO_API void
bco__on_yield(bco_t* coro, int resume_point);

BCO_API void
bco__on_terminate(bco_t* coro);

BCO_API bco_t*
bco__alloc_subcoro(bco_t* coro);

BCO_API bco_t*
bco__subcoro(bco_t* coro);

BCO_API void
bco__free_subcoro(bco_t* coro);

enum {
	bco__vars_declared = 0,
	bco__begin_declared = 0,
	bco__end_declared = 0,
};

#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BCO_IMPLEMENTATION)
#define BCO_IMPLEMENTATION
#endif

#if defined(BCO_IMPLEMENTATION) && !defined(BCO_IMPLEMENTED)
#define BCO_IMPLEMENTED

#include <string.h>

struct bco_s {
	bco_fn_t fn;
	void* args;
	char* sp;
	char* bp;
	void* userdata;
	bco_t* subcoro;
	int resume_point;
	bco_status_t status;

	_Alignas(bco_align_t) char stack[];
};

_Static_assert(_Alignof(bco_t) == _Alignof(bco_align_t), "Alignment mismatch");

bco_status_t
bco_resume(bco_t* coro) {
	if (coro->status != BCO_SUSPENDED) { return coro->status; }

	coro->status = BCO_RUNNING;
	coro->sp = coro->bp;
	coro->fn(coro, coro->args);
	BCO_ASSERT(coro->resume_point != 0, "Bare return was used to return from coroutine");
	return coro->status = coro->resume_point >= 0 ? BCO_SUSPENDED : BCO_TERMINATED;
}

size_t
bco_mem_size(size_t stack_size) {
	return sizeof(bco_t) + stack_size;
}

bco_status_t
bco_status(bco_t* coro) {
	return coro->status;
}

void
bco_terminate(bco_t* coro) {
	if (coro->status != BCO_SUSPENDED) { return; }
	if (coro->resume_point == 0) {
		coro->status = BCO_TERMINATED;
		return;
	}

	if (coro->subcoro != NULL) {
		bco_terminate(coro->subcoro);
		coro->subcoro = NULL;
	}

	coro->resume_point = -1;
	bco_resume(coro);
}

static void*
bco_relocate_ptr(bco_t* dst, bco_t* src, void* src_ptr) {
	return dst->stack + ((char*)src_ptr - src->stack);
}

void
bco_copy(bco_t* dst, bco_t* src) {
	*dst = *src;
	memcpy(dst->stack, src->stack, src->sp - src->stack);

	dst->bp = bco_relocate_ptr(dst, src, src->bp);
	dst->sp = bco_relocate_ptr(dst, src, src->sp);
	dst->args = bco_relocate_ptr(dst, src, src->args);

	if (src->subcoro != NULL) {
		dst->subcoro = bco_relocate_ptr(dst, src, src->subcoro);
		bco_copy(dst->subcoro, src->subcoro);
	}
}

void
bco_set_userdata(bco_t* coro, void* userdata) {
	coro->userdata = userdata;
	if (coro->subcoro != NULL) {
		bco_set_userdata(coro->subcoro, userdata);
	}
}

void*
bco_get_userdata(bco_t* coro) {
	return coro->userdata;
}

void*
bco__alloc(bco_t* coro, size_t size, size_t alignment) {
	size_t offset = (size_t)(coro->sp - coro->stack);
	offset = (offset + alignment - 1) & ~(alignment - 1);
	char* result = coro->stack + offset;
	coro->sp = result + size;
	return result;
}

void
bco__zero_vars(bco_t* coro) {
	memset(coro->bp, 0, coro->sp - coro->bp);
}

void
bco__spawn(bco_t* coro, bco_fn_t fn, size_t args_size, size_t args_alignment, void* args) {
	coro->resume_point = 0;
	coro->status = BCO_SUSPENDED;
	coro->subcoro = NULL;
	coro->userdata = NULL;
	coro->sp = coro->stack;
	coro->fn = fn;
	coro->args = bco__alloc(coro, args_size, args_alignment);
	coro->bp = coro->sp;
	memcpy(coro->args, args, args_size);
}

bco_t*
bco__alloc_subcoro(bco_t* coro) {
	return coro->subcoro = bco__alloc(coro, sizeof(bco_t), _Alignof(bco_t));
}

bco_t*
bco__subcoro(bco_t* coro) {
	return coro->subcoro;
}

void
bco__free_subcoro(bco_t* coro) {
	coro->sp = (char*)coro->subcoro;
	coro->subcoro = NULL;
}

int
bco__on_resume(bco_t* coro) {
	int resume_point = coro->resume_point;
#ifndef NDEBUG
	coro->resume_point = 0;  // Trip the assert in bco_resume if bare return is used
#endif
	return resume_point;
}

void
bco__on_yield(bco_t* coro, int resume_point) {
	coro->resume_point = resume_point;
}

void
bco__on_terminate(bco_t* coro) {
	coro->resume_point = -1;
}

#endif
