#ifndef BMINMAX_H
#define BMINMAX_H

/**
 * @file
 * @brief Min/Max/Clamp macros using `_Generic`.
 *
 * @ref BMIN, @ref BMAX and @ref BCLAMP dispatch to a `static inline`
 * function chosen by the type of their first argument.
 * The other arguments are converted to that type.
 * Unlike the naive macros, every argument is evaluated exactly once.
 *
 * The 13 standard arithmetic types are supported: `signed char` to
 * `unsigned long long`, `float`, `double` and `long double`.
 * Anything else (plain `char`, `bool`, pointers...) is a compile error.
 *
 * Everything is header-only so there is no implementation to define.
 */

/**
 * The smaller of two values.
 *
 * @param X the first value, its type selects the implementation
 * @param Y the second value, converted to the type of `X`
 *
 * @hideinitializer
 */
#define BMIN(X, Y) \
	_Generic((X), \
		BMINMAX__TYPES(BMINMAX__GENERIC_CASE_MIN) \
		default: bminmax__unknown \
	)((X), (Y))

/**
 * The larger of two values.
 *
 * @param X the first value, its type selects the implementation
 * @param Y the second value, converted to the type of `X`
 *
 * @hideinitializer
 */
#define BMAX(X, Y) \
	_Generic((X), \
		BMINMAX__TYPES(BMINMAX__GENERIC_CASE_MAX) \
		default: bminmax__unknown \
	)((X), (Y))

/**
 * Clamp a value to a range.
 *
 * @param VAL the value, its type selects the implementation
 * @param MIN_VAL lower bound (inclusive), converted to the type of `VAL`
 * @param MAX_VAL upper bound (inclusive), converted to the type of `VAL`
 *
 * @hideinitializer
 */
#define BCLAMP(VAL, MIN_VAL, MAX_VAL) \
	_Generic((VAL), \
		BMINMAX__TYPES(BMINMAX__GENERIC_CASE_CLAMP) \
		default: bminmax__unknown \
	)((VAL), (MIN_VAL), (MAX_VAL))

/// @cond INTERNAL

#define BMINMAX__NAME(OP, SUFFIX) bminmax__##OP##_##SUFFIX

#define BMINMAX__GENERIC_CASE(OP, TYPE, SUFFIX) \
	TYPE: BMINMAX__NAME(OP, SUFFIX),
#define BMINMAX__GENERIC_CASE_MIN(TYPE, SUFFIX) \
	BMINMAX__GENERIC_CASE(min, TYPE, SUFFIX)
#define BMINMAX__GENERIC_CASE_MAX(TYPE, SUFFIX) \
	BMINMAX__GENERIC_CASE(max, TYPE, SUFFIX)
#define BMINMAX__GENERIC_CASE_CLAMP(TYPE, SUFFIX) \
	BMINMAX__GENERIC_CASE(clamp, TYPE, SUFFIX)

#define BMINMAX__IMPL_MINMAX(OP_NAME, CMP_OP, TYPE, SUFFIX) \
	static inline TYPE \
	BMINMAX__NAME(OP_NAME, SUFFIX)(TYPE x, TYPE y) { \
		return x CMP_OP y ? x : y; \
	}
#define BMINMAX__IMPL_MIN(TYPE, SUFFIX) BMINMAX__IMPL_MINMAX(min, <, TYPE, SUFFIX)
#define BMINMAX__IMPL_MAX(TYPE, SUFFIX) BMINMAX__IMPL_MINMAX(max, >, TYPE, SUFFIX)

#define BMINMAX__IMPL_CLAMP(TYPE, SUFFIX) \
	static inline TYPE \
	BMINMAX__NAME(clamp, SUFFIX)(TYPE val, TYPE min_val, TYPE max_val) { \
		if (val < min_val) { \
			return min_val; \
		} else if (val > max_val) { \
			return max_val; \
		} else { \
			return val; \
		} \
	}

#define BMINMAX__TYPES(F) \
	F(signed char, sc) \
	F(unsigned char, uc) \
	F(signed short, ss) \
	F(unsigned short, us) \
	F(signed int, si) \
	F(unsigned int, ui) \
	F(signed long, sl) \
	F(unsigned long, ul) \
	F(signed long long, sll) \
	F(unsigned long long, ull) \
	F(float, f) \
	F(double, d) \
	F(long double, ld)

BMINMAX__TYPES(BMINMAX__IMPL_MIN)
BMINMAX__TYPES(BMINMAX__IMPL_MAX)
BMINMAX__TYPES(BMINMAX__IMPL_CLAMP)

// Selected for unsupported types: calling it with arguments does not compile
typedef struct { char dummy; } bminmax__unknown_t;

extern bminmax__unknown_t
bminmax__unknown(void);

/// @endcond

#endif
