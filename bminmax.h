#ifndef BMINMAX_H
#define BMINMAX_H

#define BMIN(X, Y) \
	_Generic((X), \
		BMINNAX__TYPES(BMINMAX__GENERIC_CASE_MIN) \
		default: bminmax__unknown \
	)((X), (Y))

#define BMAX(X, Y) \
	_Generic((X), \
		BMINNAX__TYPES(BMINMAX__GENERIC_CASE_MAX) \
		default: bminmax__unknown \
	)((X), (Y))

#define BCLAMP(VAL, MIN_VAL, MAX_VAL) \
	_Generic((VAL), \
		BMINNAX__TYPES(BMINMAX__GENERIC_CASE_CLAMP) \
		default: bminmax__unknown \
	)((VAL), (MIN_VAL), (MAX_VAL))

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

#define BMINNAX__TYPES(F) \
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

BMINNAX__TYPES(BMINMAX__IMPL_MIN)
BMINNAX__TYPES(BMINMAX__IMPL_MAX)
BMINNAX__TYPES(BMINMAX__IMPL_CLAMP)

typedef struct { char dummy; } bminmax__unknown_t;

extern bminmax__unknown_t
bminmax__unknown(void);

#endif
