#ifndef BMACRO_H
#define BMACRO_H

/**
 * @file
 * @brief Commonly used macros.
 *
 * * Type and memory helpers: @ref BCONTAINER_OF, @ref BCOUNT_OF, @ref BTYPEOF.
 * * Preprocessor helpers: @ref BCONCAT, @ref BSTRINGIFY, @ref BLIT_STRLEN.
 * * X-macro generators: @ref BENUM, @ref BMASK_ENUM.
 * * printf-style format checking: @ref BFORMAT_ATTRIBUTE, @ref BFORMAT_CHECK.
 *
 * Everything is a macro so there is no implementation to define.
 */

#include <stddef.h>

/**
 * Retrieve a pointer to the struct containing a member.
 *
 * Passing a pointer of the wrong type is a compile error.
 *
 * @param ptr pointer to the member
 * @param type the containing struct type
 * @param member name of the member in `type`
 *
 * @hideinitializer
 */
#define BCONTAINER_OF(ptr, type, member) \
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

/**
 * Number of elements in an array.
 *
 * @param X an array, not a pointer
 *
 * @hideinitializer
 */
#define BCOUNT_OF(X) (sizeof(X) / sizeof(X[0]))

/**
 * Length of a string literal at compile time, excluding the terminator.
 *
 * Anything other than a string literal is a compile error.
 *
 * @param STR a string literal
 *
 * @hideinitializer
 */
#define BLIT_STRLEN(STR) BLIT_STRLEN__(STR)
#define BLIT_STRLEN__(STR) (sizeof("" STR) - 1)

/**
 * Concatenate two tokens after expanding them.
 *
 * @hideinitializer
 */
#define BCONCAT(A, B) BCONCAT__(A,B)
#define BCONCAT__(A, B) A##B

/**
 * Turn a token into a string literal after expanding it.
 *
 * @hideinitializer
 */
#define BSTRINGIFY(X) BSTRINGIFY__(X)
#define BSTRINGIFY__(X) #X

/**
 * Define an enum and its `to_str` function from an X-macro list.
 *
 * ```c
 * #define COLORS(X) \
 *     X(COLOR_RED) \
 *     X(COLOR_GREEN)
 * BENUM(color, COLORS)
 *
 * // Expands to:
 * // typedef enum color_e { COLOR_RED, COLOR_GREEN } color_t;
 * // static inline const char* color_to_str(color_t member);
 * ```
 *
 * `NAME_to_str` returns the name of the member as a string, or NULL for a
 * value that is not part of the enum.
 *
 * @param NAME base name: `NAME_e` is the enum tag, `NAME_t` the typedef and
 *   `NAME_to_str` the function
 * @param X list macro invoking its argument once per member
 *
 * @hideinitializer
 */
#define BENUM(NAME, X) \
	typedef enum BCONCAT(NAME, _e) { \
		X(BENUM_DEFINE__) \
	} BCONCAT(NAME, _t); \
	static inline const char* BCONCAT(NAME, _to_str)(BCONCAT(NAME, _t) member) { \
		switch (member) { \
			X(BENUM_TO_STR__) \
		} \
		return (void*)0; \
	}
#define BENUM_DEFINE__(NAME) NAME,
#define BENUM_TO_STR__(NAME) case NAME: return BSTRINGIFY(NAME);

/**
 * Define bit flags from an X-macro list.
 *
 * ```c
 * #define FLAGS(X) \
 *     X(FLAG_A) \
 *     X(FLAG_B)
 * BMASK_ENUM(FLAGS)
 *
 * // FLAG_A_SHIFT == 0, FLAG_A == 1 << 0
 * // FLAG_B_SHIFT == 1, FLAG_B == 1 << 1
 * ```
 *
 * @param X list macro invoking its argument once per flag
 *
 * @hideinitializer
 */
#define BMASK_ENUM(X) \
	enum { X(BMASK_ENUM__SHIFT) }; \
	enum { X(BMASK_ENUM__MASK) };
#define BMASK_ENUM__SHIFT(NAME) BCONCAT(NAME, _SHIFT),
#define BMASK_ENUM__MASK(NAME) NAME = 1 << BCONCAT(NAME, _SHIFT),

#if defined(DOXYGEN)
/**
 * Mark a function as taking a printf-style format string so that the
 * compiler checks its arguments.
 *
 * Expands to nothing on compilers without the attribute (MSVC), see
 * @ref BFORMAT_CHECK for those.
 *
 * @param FMT 1-based index of the format string parameter
 * @param VA 1-based index of the first variadic argument
 *
 * @hideinitializer
 */
#	define BFORMAT_ATTRIBUTE(FMT, VA)
#elif defined(__GNUC__) || defined(__clang__)
#	define BFORMAT_ATTRIBUTE(FMT, VA) __attribute__((format(printf, FMT, VA)))
#else
#	define BFORMAT_ATTRIBUTE(FMT, VA)
#endif

/**
 * Check printf-style arguments on any compiler.
 *
 * The arguments are only type-checked, never evaluated, so this costs
 * nothing at runtime.
 * Typically used in a wrapper macro around a function marked with
 * @ref BFORMAT_ATTRIBUTE.
 * Requires `<stdio.h>`.
 *
 * @hideinitializer
 */
#define BFORMAT_CHECK(...) (void)(sizeof(printf(__VA_ARGS__)))

#if defined(DOXYGEN)
/**
 * The type of an expression.
 *
 * Uses C23 `typeof` where available and the compiler extension otherwise.
 *
 * @hideinitializer
 */
#	define BTYPEOF(EXP)
#elif __STDC_VERSION__ >= 202311L
#	define BTYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BTYPEOF(EXP) __typeof__(EXP)
#endif

#endif
