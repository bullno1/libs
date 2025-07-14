#ifndef BMACRO_H
#define BMACRO_H

#define BCONTAINER_OF(ptr, type, member) \
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

#define BCOUNT_OF(X) (sizeof(X) / sizeof(X[0]))

#define BLIT_STRLEN(STR) BLIT_STRLEN__(STR)
#define BLIT_STRLEN__(STR) (sizeof("" STR) - 1)

#define BCONCAT(A, B) BCONCAT__(A,B)
#define BCONCAT__(A, B) A##B

#define BSTRINGIFY(X) BSTRINGIFY__(X)
#define BSTRINGIFY__(X) #X

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

#if defined(__GNUC__) || defined(__clang__)
#	define BFORMAT_ATTRIBUTE(FMT, VA) __attribute__((format(printf, FMT, VA)))
#else
#	define BFORMAT_ATTRIBUTE(FMT, VA)
#endif

#define BFORMAT_CHECK(...) (void)(sizeof(printf(__VA_ARGS__)))

#if __STDC_VERSION__ >= 202311L
#	define BTYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BTYPEOF(EXP) __typeof__(EXP)
#endif

#endif
