#ifndef BCORO_H
#define BCORO_H

/**
 * @file
 * @brief Coroutine using macro.
 *
 * In **exactly one** source file, define `BCORO_IMPLEMENTATION` before including bcoro.h.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef BCORO_API
#define BCORO_API
#endif

/**
 * @brief Declare and implement a coroutine.
 */
#define BCORO(NAME, ARG_TYPE) \
	BCORO_DECL(NAME, ARG_TYPE) \
	BCORO_IMPL(NAME, ARG_TYPE)

/**
 * @brief Forward declare a coroutine without implementation.
 */
#define BCORO_DECL(NAME, ARG_TYPE) \
	extern void NAME(struct bcoro_s* bcoro__self, ARG_TYPE bcoro__args);

/**
 * @brief Implement a coroutine.
 */
#define BCORO_IMPL(NAME, ARG_TYPE) \
	static void bcoro__entry_##NAME(struct bcoro_s* bcoro__self, void* args); \
	static void bcoro__impl_##NAME(struct bcoro_s* bcoro__self, ARG_TYPE bcoro__args); \
	void NAME(struct bcoro_s* bcoro__self, ARG_TYPE bcoro__arg) { \
		bcoro__start(bcoro__self, bcoro__entry_##NAME, &bcoro__arg, sizeof(ARG_TYPE), _Alignof(ARG_TYPE)); \
	} \
	static inline void bcoro__entry_##NAME(struct bcoro_s* bcoro__self, void* bcoro__arg) { \
		bcoro__impl_##NAME(bcoro__self, *(ARG_TYPE*)bcoro__arg); \
	} \
	static inline void bcoro__impl_##NAME(struct bcoro_s* bcoro__self, ARG_TYPE bcoro__arg)

#define BCORO_SECTION_VARS \
	bool bcoro__yielding = false; \
	bool bcoro__cloning = false; \
	char* bcoro__sp; \
	bcoro__vars: \
	bcoro__sp = bcoro__self->vars;

/**
 * @brief Declare a variable.
 *
 * Its value will be persisted between invocations.
 */
#define BCORO_VAR(TYPE, NAME) \
	TYPE NAME; bcoro__init_var(&NAME); \
	TYPE* bcoro__var_##NAME = bcoro__alloc(bcoro__sp, sizeof(TYPE), _Alignof(TYPE)); \
	bcoro__sp = (char*)bcoro__var_##NAME + sizeof(NAME); \
	if (bcoro__yielding || bcoro__cloning) { \
		*bcoro__var_##NAME = NAME; \
	} else if (bcoro__self->resume_point != 0) { \
		NAME = *bcoro__var_##NAME; \
	}

#define BCORO_SECTION_BODY \
	BCORO_VAR(bcoro_t*, bcoro__clone) \
	if (bcoro__yielding) { bcoro__self->status = BCORO_SUSPENDED; return; } \
	bcoro_t* bcoro__subcoro = bcoro__alloc(bcoro__sp, sizeof(bcoro_t), _Alignof(bcoro_t)); \
	(void)bcoro__subcoro; \
	switch (bcoro__self->resume_point) { case 0:

#define BCORO_SECTION_CLEANUP \
	default: goto bcoro__terminate; } \
	bcoro__terminate: do { \
		bcoro__self->resume_point = -1; \
		bcoro__self->status = BCORO_TERMINATED; \
	} while (0);

/**
 * @brief Call another coroutine function.
 *
 * The called function can use @ref BCORO_YIELD to return control to the caller of this coroutine.
 */
#define BCORO_YIELD_FROM(FN, ...) \
	do { \
		FN(bcoro__subcoro, __VA_ARGS__); \
		bcoro__self->subcoro = bcoro__subcoro; \
		BCORO_JOIN(bcoro__subcoro); \
		bcoro__self->subcoro = NULL; \
	} while (0)

/**
 * @brief Wait for the other coroutine to finish.
 */
#define BCORO_JOIN(CORO) \
	while (bcoro_resume(CORO) != BCORO_TERMINATED) { \
		BCORO_YIELD(); \
	} \

/**
 * @brief Return control to the caller of this coroutine.
 */
#define BCORO_YIELD() \
	do { \
		bcoro__self->resume_point = __LINE__; \
		bcoro__yielding = true; \
		goto bcoro__vars; \
		case __LINE__:; \
	} while (0)

/**
 * @brief Terminate the currently running coroutine.
 *
 * @remarks
 *   The clean up code of this coroutine and all its descendants will be executed.
 *   @see bcoro_stop
 */
#define BCORO_EXIT() do { goto bcoro__terminate; } while (0)

/**
 * @brief The argument of this coroutine.
 */
#define BCORO_ARG bcoro__arg

/**
 * @brief The currently running coroutine.
 *
 * This should only be used as a form of identification.
 * The structure should never be modified directly.
 */
#define BCORO_SELF bcoro__self

/**
 * @brief Fork the coroutine (experimental).
 *
 * The fork will have a copy of the parent's states up to this point.
 * States are naively memcpy-ed.
 *
 * @param DEST The coroutine storage to copy to.
 *   This expression will only be evaluated once.
 * @param STACK_SIZE The stack size.
 *   This must be the same as the current stack size.
 *
 * @remarks
 *   This will transfer control to the clone.
 *   The code block that directly follows after a call to @ref BCORO_FORK will be
 *   executed under the context of the clone.
 *   This gives the clone an opportunity to create deep copies of shared resources
 *   with its parent.
 *   The clone must then call @ref BCORO_YIELD to transfer control back to its parent.
 */
#define BCORO_FORK(DEST, STACK_SIZE) \
	do { \
		bcoro__clone = (DEST); \
		bcoro__self->resume_point = __LINE__; \
		bcoro__cloning = true; \
		goto bcoro__vars; \
		case __LINE__: \
		bcoro__cloning = false; \
	} while (0); \
	if (bcoro__clone != bcoro__self) { \
		bcoro__copy(bcoro__clone, bcoro__self, STACK_SIZE); \
		bcoro__clone->status = BCORO_SUSPENDED; \
		bcoro_resume(bcoro__clone); \
	} else

/**
 * @brief The coroutine type.
 *
 * All members are read-only.
 */
typedef struct bcoro_s bcoro_t;

/**
 * @brief Coroutine status.
 */
typedef enum bcoro_status_e {
	//! The coroutine is running.
	BCORO_RUNNING,
	//! The coroutine is suspended.
	BCORO_SUSPENDED,
	//! The coroutine has finished execution.
	BCORO_TERMINATED,
} bcoro_status_t;

struct bcoro_s {
	bcoro_status_t status;
	int resume_point;
	bcoro_t* subcoro;
	void* args;
	void* vars;
	void (*fn)(bcoro_t* self, void* args);
	char stack[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the memory needed for the coroutine given the stack size.
 */
BCORO_API size_t
bcoro_mem_size(size_t stack_size);

/**
 * @brief Stop a running coroutine.
 *
 * @param coro The coroutine to stop.
 * @return Its new status.
 *
 * @remarks
 *   This is only effective in the @ref BCORO_SUSPENDED state.
 *   This function will return @ref BCORO_TERMINATED after the cleanup code of
 *   this coroutine and its sub-coroutines have finished execution.
 *   Cleanup code is run from the innermost coroutine to the outermost coroutine.
 *
 *   In other states, this is a noop that returns the current state.
 */
BCORO_API bcoro_status_t
bcoro_stop(bcoro_t* coro);

/**
 * @brief Resume a coroutine.
 *
 * @param coro The coroutine to stop.
 * @return Its new status.
 *
 * @remarks
 *   It is important to check the returned value to check whether the coroutine
 *   should be scheduled for resumption in the future.
 */
BCORO_API bcoro_status_t
bcoro_resume(bcoro_t* coro);

/**
 * @brief Get a coroutine's status.
 */
BCORO_API bcoro_status_t
bcoro_status(bcoro_t* coro);

/**
 * @brief Initialize a noop coroutine.
 *
 * Rather than leaving a @ref bcoro_t uninitialized, this makes the coroutine
 * safe to resume.
 * It will immediately terminate upon being resumed.
 *
 * @param coro The coroutine to initialize.
 */
BCORO_API void
bcoro_noop(bcoro_t* coro);

#ifndef DOXYGEN

// Private functions, should not be called directly.

BCORO_API void
bcoro__start(
	bcoro_t* coro,
	void(*fn)(bcoro_t* self, void* args),
	void* args,
	size_t args_size,
	size_t args_alignment
);

BCORO_API void
bcoro__copy(bcoro_t* dest, const bcoro_t* src, size_t stack_size);

static inline void*
bcoro__alloc(char* sp, size_t size, size_t alignment) {
	return (void*)(((intptr_t)sp + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

static inline void
bcoro__init_var(void* var) {
	(void)var;
}

#endif

#ifdef __cplusplus
}
#endif

#endif

#ifdef BCORO_IMPLEMENTATION

#include <string.h>

size_t
bcoro_mem_size(size_t stack_size) {
	return sizeof(bcoro_t) + stack_size;
}

void
bcoro__start(
	bcoro_t* coro,
	void(*fn)(bcoro_t* self, void* args),
	void* args,
	size_t args_size,
	size_t args_alignment
) {
	coro->resume_point = 0;
	coro->args = bcoro__alloc(coro->stack, args_size, args_alignment);
	memcpy(coro->args, args, args_size);
	coro->vars = (char*)coro->args + args_size;
	coro->fn = fn;
	coro->subcoro = NULL;
	coro->status = BCORO_SUSPENDED;
}

bcoro_status_t
bcoro_stop(bcoro_t* coro) {
	if (coro->status != BCORO_SUSPENDED) { return coro->status; }

	if (coro->subcoro != NULL) { bcoro_stop(coro->subcoro); }

	coro->resume_point = -1;
	return bcoro_resume(coro);
}

bcoro_status_t
bcoro_resume(bcoro_t* coro) {
	if (coro->status != BCORO_SUSPENDED) { return coro->status; }

	coro->status = BCORO_RUNNING;
	coro->fn(coro, coro->args);
	return coro->status;
}

bcoro_status_t
bcoro_status(bcoro_t* coro) {
	return coro->status;
}

static void
bcoro__noop_fn(bcoro_t* self, void* arg) {
	(void)arg;
	self->resume_point = -1;
	self->status = BCORO_TERMINATED;
}

void
bcoro_noop(bcoro_t* coro) {
	coro->resume_point = 0;
	coro->args = coro->stack;
	coro->vars = coro->stack;
	coro->fn = bcoro__noop_fn;
	coro->subcoro = NULL;
	coro->status = BCORO_SUSPENDED;
}

void
bcoro__copy(bcoro_t* dest, const bcoro_t* src, size_t stack_size) {
	ptrdiff_t args_offset = (intptr_t)src->args - (intptr_t)src->stack;
	ptrdiff_t vars_offset = (intptr_t)src->vars - (intptr_t)src->stack;
	dest->args = dest->stack + args_offset;
	dest->vars = dest->stack + vars_offset;
	dest->resume_point = src->resume_point;
	dest->subcoro = NULL;
	dest->fn = src->fn;
	dest->status = src->status;

	memcpy(dest->stack, src->stack, stack_size);
}

#endif
