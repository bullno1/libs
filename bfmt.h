// vim: set foldmethod=marker foldlevel=0:
#ifndef BFMT_H
#define BFMT_H

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef BFMT_API
#define BFMT_API
#endif

#ifndef BFMT_USER_TYPES
#define BFMT_USER_TYPES(X)
// #define BFMT_USER_TYPES(X) X(TYPE, FORMATTER, OPTIONS)
#endif

#define bfmt_print(OUT, ...) \
	bfmt__print(OUT, (bfmt__element_t[]){ bfmt__map(bfmt__to_element, __VA_ARGS__) {} })

#define bfmt_println(OUT, ...) bfmt_print(OUT, __VA_ARGS__ __VA_OPT__(,) "\n")

#ifdef BFMT_EXTRACT

/*
	xgettext \
		--from-code=UTF-8 \
		--add-comments=TRANSLATORS: \
		--flag=gettext:1:no-c-format \
		--flag=pgettext:2:no-c-format \
		--sort-by-file \
		--language=C
*/


#define bfmt_translate(OUT, LOCALE, ...) bfmt_text(__VA_ARGS__)
#define bfmt_ptranslate(OUT, LOCALE, CONTEXT, ...) bfmt_ptext(CONTEXT, __VA_ARGS__)
#define bfmt_text(...) gettext(bfmt__map(bfmt__to_template_text, __VA_ARGS__))
#define bfmt_ptext(CONTEXT, ...) pgettext(CONTEXT, bfmt__map(bfmt__to_template_text, __VA_ARGS__))

#else

#define bfmt_translate(OUT, LOCALE, ...) bfmt_ptranslate(OUT, LOCALE, NULL, __VA_ARGS__)

#define bfmt_ptranslate(OUT, LOCALE, CONTEXT, ...) \
	( \
		bfmt__map(bfmt__check_arg, __VA_ARGS__) \
		bfmt__ptranslate( \
			OUT, \
			LOCALE, \
			CONTEXT, \
			bfmt__map(bfmt__to_template_text, __VA_ARGS__), \
			(bfmt__template_input_t[]){ bfmt__map(bfmt__to_template_input, __VA_ARGS__) {} } \
		) \
	)

#define bfmt_text(...) bfmt_ptext(NULL, __VA_ARGS__)

#define bfmt_ptext(CONTEXT, ...) (bfmt_text_t){ .context = CONTEXT, .content = bfmt__map(bfmt__to_template_text, __VA_ARGS__) }

#endif

#define bfmt_named(NAME, VALUE) (bfmt_named, NAME, VALUE)
#define bfmt_plural(NAME, NUMBER, ...) (bfmt__switch, plural, NAME, NUMBER, __VA_ARGS__)
#define bfmt_ordinal(NAME, NUMBER, ...) (bfmt__switch, selectordinal, NAME, NUMBER, __VA_ARGS__)
#define bfmt_select(NAME, SELECTOR, ...) (bfmt__switch, select, NAME, SELECTOR, __VA_ARGS__)

#define bfmt_register_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE) \
	typedef VALUE_TYPE bfmt__formatter_value_type(FORMATTER_NAME); \
	typedef OPTIONS_TYPE bfmt__formatter_options_type(FORMATTER_NAME); \
	bfmt__make_wrapper(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE)

#define bfmt_decl_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE) \
	extern bfmt_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE); \
	bfmt_register_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE)

#define bfmt_formatter(VALUE_TYPE, NAME, OPTIONS_TYPE) \
	void NAME(bfmt_ctx_t* ctx, VALUE_TYPE value, OPTIONS_TYPE options)

#define bfmt_with(FORMATTER, VALUE, ...) \
	( \
		(void)sizeof((bfmt__typeof(FORMATTER((bfmt_ctx_t*)0, VALUE, bfmt__options(bfmt__formatter_options_type(FORMATTER), __VA_ARGS__)[0]))*)0), \
		(bfmt__element_t){ \
			.value = (bfmt__formatter_value_type(FORMATTER)[1]){ VALUE }, \
			.formatter = bfmt__formatter_wrapper(FORMATTER), \
			.options = bfmt__options(bfmt__formatter_options_type(FORMATTER), __VA_ARGS__), \
		} \
	)

#define bfmt(VALUE, ...) \
	((bfmt__element_t){ \
		.value = (bfmt__typeof_decay(VALUE)[1]){ VALUE }, \
		.formatter = bfmt__formatter_for(VALUE), \
		.options = bfmt__options(bfmt__options_for(VALUE), __VA_ARGS__), \
	})

#define bfmt_precision(PRECISION) (bfmt_precision_t){ .enabled = true, .precision = PRECISION }

typedef struct bfmt_ctx_s bfmt_ctx_t;

typedef enum {
	BFMT_PLURAL_ZERO,
	BFMT_PLURAL_ONE,
	BFMT_PLURAL_TWO,
	BFMT_PLURAL_FEW,
	BFMT_PLURAL_MANY,
	BFMT_PLURAL_OTHER,
} bfmt_plural_t;

typedef enum {
	BFMT_PLURAL_CARDINAL,
	BFMT_PLURAL_ORDINAL,
} bfmt_plural_type_t;

typedef struct {
	uint64_t i;  /* integer digits */
	uint64_t f;  /* visible fraction digits, as an integer */
	uint64_t t;  /* f without trailing zeros */
	unsigned v;  /* number of visible fraction digits */
	unsigned w;  /* v without trailing zeros */
} bfmt_plural_num_t;

typedef bfmt_plural_t bfmt_plural_rule_t(bfmt_plural_num_t number, bfmt_plural_type_t type);

typedef struct {
	bfmt_plural_rule_t* plural_rule;
	const char* (*translate)(void* userdata, const char* ctx, const char* key);
	// Optional. Called when `template` can not be rendered, with the byte
	// offset of the error. `template` is what `translate` returned for
	// (ctx, key), or `key` itself when there is no translation.
	// A typical handler drops the mapping so that the source text is used
	// from then on.
	void (*report_error)(void* userdata, const char* ctx, const char* key, const char* template, int offset);
	void* userdata;
} bfmt_locale_t;

typedef struct {
	const char* content;
	const char* context;
} bfmt_text_t;

typedef struct {
	void (*write)(void* userdata, const char* string, int len);
	void* userdata;
} bfmt_stream_t;

typedef struct {
	char bfmt__unused;
} bfmt_no_options_t;

typedef enum {
	BFMT_ALIGN_RIGHT,
	BFMT_ALIGN_LEFT,
} bfmt_alignment_t;

typedef struct {
	int min_width;
	bfmt_alignment_t align;
} bfmt_layout_t;

typedef enum {
	BFMT_SIGN_IF_NEEDED,       // " 1", "-1"
	BFMT_SIGN_ALWAYS,          // "+1", "-1"
	BFMT_SIGN_SPACE_IF_PLUS,   // " 1", "-1"
} bfmt_sign_style_t;

typedef enum {
	BFMT_BASE_DEC,
	BFMT_BASE_HEX,
	BFMT_BASE_OCT,
} bfmt_base_t;

typedef enum {
    BFMT_FLOAT_FIXED,       // 123.456 or 1.23456e+02
    BFMT_FLOAT_EXPONENT,    // 1.23456e+02
    BFMT_FLOAT_GENERAL,     // 123.456
    BFMT_FLOAT_HEX,         // 0x1.edd2f1p+6
} bfmt_float_style_t;

typedef struct {
	bool enabled;
	int precision;
} bfmt_precision_t;

typedef struct {
	bfmt_layout_t layout;
	bfmt_sign_style_t sign;
	bfmt_base_t base;
	bfmt_precision_t precision;
	bool show_base;
	bool zero_pad;
	bool uppercase;
} bfmt_int_options_t;

typedef struct {
	bfmt_layout_t layout;
	bfmt_sign_style_t sign;
	bfmt_float_style_t style;
	bfmt_precision_t precision;
	bool show_base;
	bool zero_pad;
	bool uppercase;
} bfmt_float_options_t;

typedef struct {
	bfmt_layout_t layout;
	bfmt_precision_t precision;
} bfmt_str_options_t;

typedef struct {
	bfmt_layout_t layout;
} bfmt_simple_options_t;

extern bfmt_stream_t* bfmt_stdout;
extern bfmt_stream_t* bfmt_stderr;

BFMT_API void
bfmt_write(bfmt_ctx_t* ctx, const char* str, int len);

BFMT_API void
bfmt_fmt(bfmt_ctx_t* ctx, const char* fmt, ...);

BFMT_API void
bfmt_fmtv(bfmt_ctx_t* ctx, const char* fmt, va_list args);

BFMT_API bfmt_stream_t
bfmt_wrap_file(FILE* file);

BFMT_API void
bfmt_translate_text(
	bfmt_stream_t* stream,
	bfmt_locale_t* locale,
	const bfmt_text_t* text
);

// Internal {{{

// Macro helpers {{{

#define bfmt__map(F, ...) __VA_OPT__(bfmt__expand(bfmt__map_helper(F, __VA_ARGS__)))
#define bfmt__map_helper(F, ARG, ...) F(ARG) __VA_OPT__(bfmt__map_again bfmt__parens (F, __VA_ARGS__))
#define bfmt__map_again() bfmt__map_helper
#define bfmt__expand(...)  bfmt__expand3(bfmt__expand3(__VA_ARGS__))
#define bfmt__expand3(...) bfmt__expand2(bfmt__expand2(__VA_ARGS__))
#define bfmt__expand2(...) bfmt__expand1(bfmt__expand1(__VA_ARGS__))
#define bfmt__expand1(...) __VA_ARGS__

#define bfmt__map2(F, ...) __VA_OPT__(bfmt__expand_2(bfmt__map_helper2(F, __VA_ARGS__)))
#define bfmt__map_helper2(F, ARG, ...) F(ARG) __VA_OPT__(bfmt__map_again2 bfmt__parens (F, __VA_ARGS__))
#define bfmt__map_again2() bfmt__map_helper2
#define bfmt__expand_2(...)  bfmt__expand3_2(bfmt__expand3_2(__VA_ARGS__))
#define bfmt__expand3_2(...) bfmt__expand2_2(bfmt__expand2_2(__VA_ARGS__))
#define bfmt__expand2_2(...) bfmt__expand1_2(bfmt__expand1_2(__VA_ARGS__))
#define bfmt__expand1_2(...) __VA_ARGS__

#define bfmt__parens ()


#define bfmt__is_paren(X) bfmt__check_paren(bfmt__probe_paren X)
#define bfmt__probe_paren(...) ~, 1,
#define bfmt__check_paren(...) bfmt__check_paren_n(__VA_ARGS__, 0, )
#define bfmt__check_paren_n(X, N, ...) N

#define bfmt__concat(LHS, RHS) bfmt__concat2(LHS, RHS)
#define bfmt__concat2(LHS, RHS) LHS##RHS

#if __STDC_VERSION__ >= 202311L
#	define bfmt__typeof(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define bfmt__typeof(EXP) __typeof__(EXP)
#endif

#define bfmt__typeof_decay(VALUE) bfmt__typeof(0 ? (VALUE) : (VALUE))

// }}}

// Format {{{

typedef void bfmt__formatter_t(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref);

typedef struct {
	const void* value;
	bfmt__formatter_t* formatter;
	const void* options;
} bfmt__element_t;

// Wrapper macro for bfmt_fmt
// The unexecuted printf forces the compiler to check the correctness of
// printf arguments
#define bfmt_fmt(ctx, ...) \
	((void)sizeof(printf(__VA_ARGS__)), bfmt_fmt(ctx, __VA_ARGS__))

#define bfmt__builtin_value_types(X) \
	X(char              , bfmt__format_char      , bfmt_simple_options_t) \
	X(short             , bfmt__format_short     , bfmt_int_options_t) \
	X(unsigned short    , bfmt__format_ushort    , bfmt_int_options_t) \
	X(int               , bfmt__format_int       , bfmt_int_options_t) \
	X(unsigned int      , bfmt__format_uint      , bfmt_int_options_t) \
	X(long              , bfmt__format_long      , bfmt_int_options_t) \
	X(unsigned long     , bfmt__format_ulong     , bfmt_int_options_t) \
	X(long long         , bfmt__format_longlong  , bfmt_int_options_t) \
	X(unsigned long long, bfmt__format_ulonglong , bfmt_int_options_t) \
	X(float             , bfmt__format_float     , bfmt_float_options_t) \
	X(double            , bfmt__format_double    , bfmt_float_options_t) \
	X(long double       , bfmt__format_longdouble, bfmt_float_options_t) \
	X(bool              , bfmt__format_bool      , bfmt_str_options_t) \
	X(bfmt__element_t   , bfmt__format_element   , bfmt_no_options_t)

#define bfmt__builtin_types(X) \
	X(const char*       , bfmt__format_str       , bfmt_str_options_t) \
	X(char*             , bfmt__format_str       , bfmt_str_options_t) \
	X(const void*       , bfmt__format_ptr       , bfmt_simple_options_t) \
	X(void*             , bfmt__format_ptr       , bfmt_simple_options_t) \
	bfmt__builtin_value_types(X)

#define bfmt__all_types(X) bfmt__builtin_types(X) BFMT_USER_TYPES(X)

#define bfmt__formatter_for(VALUE) _Generic(VALUE bfmt__all_types(bfmt__type_to_formatter))
#define bfmt__type_to_formatter(TYPE, FORMATTER, OPTIONS) , TYPE: bfmt__formatter_wrapper(FORMATTER)

#define bfmt__options_for(VALUE) bfmt__typeof(*_Generic(VALUE bfmt__all_types(bfmt__type_to_options)))
#define bfmt__type_to_options(TYPE, FORMATTER, OPTIONS) , TYPE: (OPTIONS*)0

#define bfmt__formatter_value_type(FORMATTER_NAME) bfmt__concat(bfmt__type_, FORMATTER_NAME)
#define bfmt__formatter_options_type(FORMATTER_NAME) bfmt__concat(bfmt__options_, FORMATTER_NAME)
#define bfmt__formatter_wrapper(FORMATTER_NAME) bfmt__concat(bfmt__wrapper_, FORMATTER_NAME)

#define bfmt__declare_formatter(TYPE, FORMATTER, OPTIONS) \
	BFMT_API void FORMATTER(bfmt_ctx_t* ctx, TYPE value, OPTIONS options); \
	bfmt__make_wrapper(TYPE, FORMATTER, OPTIONS)

#define bfmt__make_wrapper(TYPE, FORMATTER, OPTIONS) \
	static inline void bfmt__formatter_wrapper(FORMATTER)(bfmt_ctx_t* ctx, const void* value, const void* options) { \
		FORMATTER(ctx, *(TYPE*)value, *(OPTIONS*)options); \
	}

bfmt__builtin_value_types(bfmt__declare_formatter)

BFMT_API void
bfmt__format_str(bfmt_ctx_t* ctx, const char* value, bfmt_str_options_t options);
bfmt__make_wrapper(const char*, bfmt__format_str, bfmt_str_options_t)

BFMT_API void
bfmt__format_ptr(bfmt_ctx_t* ctx, const void* value, bfmt_simple_options_t options);
bfmt__make_wrapper(const void*, bfmt__format_ptr, bfmt_simple_options_t)

#define bfmt__to_element(VALUE) bfmt(VALUE),

// This accepts all of:
//
// * A value of type `TYPE`
// * An initializer list
// * Nothing
//
// And it decays into TYPE*
#define bfmt__options(TYPE, ...) ((TYPE[1]){ __VA_OPT__([0] = __VA_ARGS__) })

// }}}

// Translation {{{

#define bfmt__to_template_text(X) bfmt__concat(bfmt__to_template_text_, bfmt__is_paren(X))(X)
#define bfmt__to_template_text_0(X) X
#define bfmt__to_template_text_1(X) bfmt__to_template_text_apply X
#define bfmt__to_template_text_apply(HEAD, ...) \
	bfmt__concat(bfmt__to_template_text_, HEAD)(__VA_ARGS__)

#define bfmt__to_template_text_bfmt_named(NAME, VALUE) "{" #NAME "}"
#define bfmt__to_template_text_bfmt__switch(ARG_TYPE, NAME, VALUE, ...) \
	"{" #NAME ", " #ARG_TYPE "," bfmt__map2(bfmt__switch_case_to_template_text, __VA_ARGS__) "}"

#define bfmt__switch_case_to_template_text(X) bfmt__switch_case_to_template_text2 X
#define bfmt__switch_case_to_template_text2(CASE, TEXT) " " #CASE "{" TEXT "}"

#define bfmt__to_template_input(X) bfmt__concat(bfmt__to_template_input_, bfmt__is_paren(X))(X)
#define bfmt__to_template_input_0(X)
#define bfmt__to_template_input_1(X) bfmt__to_template_input_apply X
#define bfmt__to_template_input_apply(HEAD, ...) \
	bfmt__concat(bfmt__to_template_input_, HEAD)(__VA_ARGS__)

#define bfmt__to_template_input_bfmt_named(NAME, VALUE) bfmt__template_input(#NAME, VALUE),
#define bfmt__to_template_input_bfmt__switch(ARG_TYPE, NAME, VALUE, ...) bfmt__template_input(#NAME, VALUE),

#define bfmt__template_input(NAME, VALUE) { NAME, bfmt(VALUE), bfmt__value_kind(VALUE) }

// The same promoted type that `bfmt` stores the value as
#define bfmt__value_kind(VALUE) \
	_Generic(0 ? (VALUE) : (VALUE), \
		const char*: BFMT__VALUE_STR, \
		char*: BFMT__VALUE_STR, \
		int: BFMT__VALUE_INT, \
		unsigned int: BFMT__VALUE_UINT, \
		long: BFMT__VALUE_LONG, \
		unsigned long: BFMT__VALUE_ULONG, \
		long long: BFMT__VALUE_LLONG, \
		unsigned long long: BFMT__VALUE_ULLONG, \
		default: BFMT__VALUE_OTHER \
	)

#define bfmt__check_arg(X) bfmt__concat(bfmt__check_arg_, bfmt__is_paren(X))(X)
#define bfmt__check_arg_0(X) (void)sizeof(struct { _Static_assert(bfmt__is_literal_str(X), "`" #X "` is not a literal string"); char bfmt__unused; }),
#define bfmt__check_arg_1(X) (void)0,

#define bfmt__is_literal_str(X) \
	_Generic((bfmt__typeof_decay(X)){}, const char*: 1, char*: 1, default: 0)

// Lets the renderer read plural counts and select keys out of an element
// without formatting them first
typedef enum {
	BFMT__VALUE_OTHER,
	BFMT__VALUE_STR,
	BFMT__VALUE_INT,
	BFMT__VALUE_UINT,
	BFMT__VALUE_LONG,
	BFMT__VALUE_ULONG,
	BFMT__VALUE_LLONG,
	BFMT__VALUE_ULLONG,
} bfmt__value_kind_t;

typedef struct {
	const char* name;
	bfmt__element_t element;
	bfmt__value_kind_t kind;
} bfmt__template_input_t;

#define bfmt__ptranslate(OUT, ELEMENTS, CONTEXT, TEMPLATE, INPUT) \
	_Generic(OUT, \
		bfmt_stream_t*: bfmt__ptranslate_stream, \
		bfmt_ctx_t*: bfmt__ptranslate_ctx \
	)(OUT, ELEMENTS, CONTEXT, TEMPLATE, INPUT)

BFMT_API void
bfmt__ptranslate_stream(
	bfmt_stream_t* stream,
	bfmt_locale_t* locale,
	const char* context,
	const char* template,
	const bfmt__template_input_t* inputs
);

BFMT_API void
bfmt__ptranslate_ctx(
	bfmt_ctx_t* ctx,
	bfmt_locale_t* locale,
	const char* context,
	const char* template,
	const bfmt__template_input_t* inputs
);

// }}}

// Stream output {{{

#define bfmt__print(OUT, ELEMENTS) \
	_Generic(OUT, \
		bfmt_stream_t*: bfmt__print_stream, \
		bfmt_ctx_t*: bfmt__print_ctx \
	)(OUT, ELEMENTS)

BFMT_API void
bfmt__print_stream(bfmt_stream_t* stream, const bfmt__element_t* elements);

BFMT_API void
bfmt__print_ctx(bfmt_ctx_t* ctx, const bfmt__element_t* elements);

// }}}

// }}}

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BFMT_IMPLEMENTATION)
#define BFMT_IMPLEMENTATION
#endif

#if defined(BFMT_IMPLEMENTATION) && !defined(BFMT_IMPLEMENTED)
#define BFMT_IMPLEMENTED

#include <string.h>

// Format {{{

#define bfmt_printf(CTX, OPTIONS, ARG) \
	do { \
		if (OPTIONS.layout.min_width > 0 && OPTIONS.precision.enabled) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.layout.min_width, OPTIONS.precision.precision, ARG); \
		} else if (OPTIONS.layout.min_width > 0) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.layout.min_width, ARG); \
		} else if (OPTIONS.precision.enabled) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.precision.precision, ARG); \
		} else { \
			(bfmt_fmt)(ctx, fmt, ARG); \
		} \
	} while (0)

typedef struct {
	bfmt_layout_t layout;
	bfmt_sign_style_t sign;
	bfmt_precision_t precision;
	bool show_base;
	bool zero_pad;
	bool uppercase;
} bfmt_options_t;

static void
bfmt_build_fmt(char* cursor, const char* length, char specifier, bfmt_options_t options) {
	*cursor++ = '%';

	if (options.layout.align == BFMT_ALIGN_LEFT) {
		*cursor++ = '-';
	}

	switch (options.sign) {
		case BFMT_SIGN_IF_NEEDED:
			break;
		case BFMT_SIGN_ALWAYS:
			*cursor++ = '+';
			break;
		case BFMT_SIGN_SPACE_IF_PLUS:
			*cursor++ = ' ';
			break;
	}

	if (options.show_base) {
		*cursor++ = '#';
	}

	if (options.zero_pad) {
		*cursor++ = '0';
	}

	if (options.layout.min_width > 0) {
		*cursor++ = '*';
	}

	if (options.precision.enabled) {
		*cursor++ = '.';
		*cursor++ = '*';
	}

	while (*length != '\0') {
		*cursor++ = *length++;
	}

	*cursor++ = specifier;
	*cursor++ = '\0';
}

static char
bfmt__int_specifier(bfmt_int_options_t options, char decimal) {
	switch (options.base) {
		case BFMT_BASE_DEC: return decimal;
		case BFMT_BASE_OCT: return 'o';
		case BFMT_BASE_HEX: return options.uppercase ? 'X' : 'x';
		default: return decimal;
	}
}

static char
bfmt__float_specifier(bfmt_float_options_t options) {
	switch(options.style) {
		case BFMT_FLOAT_FIXED: return options.uppercase ? 'F' : 'f';
		case BFMT_FLOAT_EXPONENT: return options.uppercase ? 'E' : 'e';
		case BFMT_FLOAT_HEX: return options.uppercase ? 'A' : 'a';
		case BFMT_FLOAT_GENERAL: return options.uppercase ? 'G' : 'g';
		default: return 'f';
	}
}

void
bfmt__format_str(bfmt_ctx_t* ctx, const char* value, bfmt_str_options_t options) {
	if (options.precision.enabled || options.layout.min_width > 0) {  // Fancy formatting
		char fmt[16];
		bfmt_options_t fmt_options = {
			.layout = options.layout,
			.precision = options.precision,
		};
		bfmt_build_fmt(fmt, "", 's', fmt_options);
		bfmt_printf(ctx, fmt_options, value);
	} else {  // Direct write
		bfmt_write(ctx, value, (int)strlen(value));
	}
}

void
bfmt__format_char(bfmt_ctx_t* ctx, char value, bfmt_simple_options_t options) {
	char fmt[16];
	bfmt_options_t fmt_options = {
		.layout = options.layout,
	};
	bfmt_build_fmt(fmt, "", 'c', fmt_options);
	bfmt_printf(ctx, fmt_options, value);
}

void
bfmt__format_ptr(bfmt_ctx_t* ctx, const void* value, bfmt_simple_options_t options) {
	char fmt[16];
	bfmt_options_t fmt_options = {
		.layout = options.layout,
	};
	bfmt_build_fmt(fmt, "", 'p', fmt_options);
	bfmt_printf(ctx, fmt_options, value);
}

void
bfmt__format_bool(bfmt_ctx_t* ctx, bool value, bfmt_str_options_t options) {
	bfmt__format_str(ctx, value ? "true" : "false", options);
}

#define bfmt__format_int_type(TYPE, SUFFIX, LENGTH, SPECIFIER) \
	void bfmt__concat(bfmt__format_, SUFFIX)(bfmt_ctx_t* ctx, TYPE value, bfmt_int_options_t options) { \
		char fmt[16]; \
		bfmt_options_t fmt_options = { \
			.layout = options.layout, \
			.sign = options.sign, \
			.precision = options.precision, \
			.show_base = options.show_base, \
			.zero_pad = options.zero_pad, \
			.uppercase = options.uppercase, \
		}; \
		bfmt_build_fmt(fmt, LENGTH, bfmt__int_specifier(options, SPECIFIER), fmt_options); \
		bfmt_printf(ctx, fmt_options, value); \
	}

bfmt__format_int_type(short, short, "h", 'd')
bfmt__format_int_type(unsigned short, ushort, "h", 'u')
bfmt__format_int_type(int, int, "", 'd')
bfmt__format_int_type(unsigned int, uint, "", 'u')
bfmt__format_int_type(long, long, "l", 'd')
bfmt__format_int_type(unsigned long, ulong, "l", 'u')
bfmt__format_int_type(long long, longlong, "ll", 'd')
bfmt__format_int_type(unsigned long long, ulonglong, "ll", 'u')

#define bfmt__format_float_type(TYPE, SUFFIX, LENGTH) \
	void bfmt__concat(bfmt__format_, SUFFIX)(bfmt_ctx_t* ctx, TYPE value, bfmt_float_options_t options) { \
		char fmt[16]; \
		bfmt_options_t fmt_options = { \
			.layout = options.layout, \
			.sign = options.sign, \
			.precision = options.precision, \
			.show_base = options.show_base, \
			.zero_pad = options.zero_pad, \
			.uppercase = options.uppercase, \
		}; \
		bfmt_build_fmt(fmt, LENGTH, bfmt__float_specifier(options), fmt_options); \
		bfmt_printf(ctx, fmt_options, value); \
	}

bfmt__format_float_type(float, float, "")
bfmt__format_float_type(double, double, "")
bfmt__format_float_type(long double, longdouble, "L")

void
bfmt__format_element(bfmt_ctx_t* ctx, bfmt__element_t element, bfmt_no_options_t options) {
	(void)options;
	element.formatter(ctx, element.value, element.options);
}

// }}}

// Stream output {{{

struct bfmt_ctx_s {
	int bytes_buffered;
	bfmt_stream_t* stream;
	char buf[256];
};

void
bfmt__print_stream(bfmt_stream_t* stream, const bfmt__element_t* elements) {
	bfmt_ctx_t ctx = { .stream = stream };
	bfmt__print_ctx(&ctx, elements);
	if (ctx.bytes_buffered > 0) {
		stream->write(stream->userdata, ctx.buf, ctx.bytes_buffered);
	}
}

void
bfmt__print_ctx(bfmt_ctx_t* ctx, const bfmt__element_t* elements) {
	for (const bfmt__element_t* itr = elements; itr->formatter != NULL; ++itr) {
		itr->formatter(ctx, itr->value, itr->options);
	}
}

void
bfmt_write(bfmt_ctx_t* ctx, const char* str, int len) {
	int buffer_capacity = (int)sizeof(ctx->buf);
	int buffer_free = buffer_capacity - ctx->bytes_buffered;

	if (buffer_free < len) { // Flush if not enough space
		ctx->stream->write(ctx->stream->userdata, ctx->buf, ctx->bytes_buffered);
		ctx->bytes_buffered = 0;
		buffer_free = buffer_capacity;
	}

	if (buffer_free >= len) {  // Buffer if there is space
		memcpy(&ctx->buf[ctx->bytes_buffered], str, len);
		ctx->bytes_buffered += len;
	} else {  // Or write directly
		ctx->stream->write(ctx->stream->userdata, str, len);
	}
}

void
(bfmt_fmt)(bfmt_ctx_t* ctx, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bfmt_fmtv(ctx, fmt, args);
	va_end(args);
}


typedef void (*npf_putc)(int c, void *ctx);

static int npf_vpprintf(
	npf_putc pc,
	void* restrict pc_ctx,
	char const* restrict format,
	va_list vlist
);

static void
bfmt_putc(int c, void* userdata) {
	bfmt_ctx_t* ctx = userdata;

	if (ctx->bytes_buffered == (int)sizeof(ctx->buf)) {
		ctx->stream->write(ctx->stream->userdata, ctx->buf, ctx->bytes_buffered);
		ctx->bytes_buffered = 0;
	}

	ctx->buf[ctx->bytes_buffered++] = (char)c;
}

void
bfmt_fmtv(bfmt_ctx_t* ctx, const char* fmt, va_list args) {
	npf_vpprintf(bfmt_putc, ctx, fmt, args);
}

static void
bfmt__write_file(void* userdata, const char* str, int len) {
	FILE* file = userdata;

	size_t bytes_left = (size_t)len;
	while (bytes_left > 0) {
		size_t bytes_written = fwrite(str, 1, bytes_left, file);
		if (bytes_written == 0) { break; }
		bytes_left -= bytes_written;
		str += bytes_written;
	}
}

bfmt_stream_t
bfmt_wrap_file(FILE* file) {
	return (bfmt_stream_t){
		.userdata = file,
		.write = bfmt__write_file,
	};
}

static void
bfmt_write_stdout(void* userdata, const char* str, int len) {
	(void)userdata;
	bfmt__write_file(stdout, str, len);
}

static void
bfmt_write_stderr(void* userdata, const char* str, int len) {
	(void)userdata;
	bfmt__write_file(stderr, str, len);
}

bfmt_stream_t* bfmt_stdout = &(bfmt_stream_t){
	.write = bfmt_write_stdout,
};

bfmt_stream_t* bfmt_stderr = &(bfmt_stream_t){
	.write = bfmt_write_stderr,
};

// }}}

// Translation {{{

void
bfmt_translate_text(
	bfmt_stream_t* stream,
	bfmt_locale_t* locale,
	const bfmt_text_t* text
) {
	bfmt__ptranslate_stream(stream, locale, text->context, text->content, (bfmt__template_input_t[1]){});
}

void
bfmt__ptranslate_stream(
	bfmt_stream_t* stream,
	bfmt_locale_t* locale,
	const char* context,
	const char* template,
	const bfmt__template_input_t* inputs
) {
	bfmt_ctx_t ctx = { .stream = stream };
	bfmt__ptranslate_ctx(&ctx, locale, context, template, inputs);
	if (ctx.bytes_buffered > 0) {
		stream->write(stream->userdata, ctx.buf, ctx.bytes_buffered);
	}
}

// Template renderer for the ICU MessageFormat subset produced by the macros:
//
//   message   = text ( '{' argument '}' text )*
//   argument  = name
//             | name ',' ( "plural" | "selectordinal" | "select" ) ',' cases
//   cases     = ( selector '{' message '}' )+       ; must include `other`
//   selector  = keyword | '=' digits                ; `=N` only for plural kinds
//
// Whitespace around names, types and selectors is ignored. `#` inside a plural
// case is the number. `''` is a literal apostrophe. An apostrophe followed by
// `{`, `}` or `#` quotes the text up to the next lone apostrophe. Unlike ICU,
// this holds everywhere and not only inside a plural case for `#`.
// Anything else, `offset:` included, is unsupported and is a syntax error.
//
// The template is rendered in a single pass with no prior validation. Cases
// that are not selected are only skipped, so an error inside one goes
// unnoticed until it is selected. On the first error, rendering stops, the
// rest of the template is written out verbatim starting from the offending
// argument and `bfmt_locale_t::report_error` is called.

typedef struct {
	bfmt_ctx_t* out;
	bfmt_locale_t* locale;
	const bfmt__template_input_t* inputs;
	const char* error;  // Position of the first error, NULL if none
	const char* resume;  // Where verbatim output starts after an error
} bfmt__tpl_t;

typedef struct {
	char buf[64];
	int len;
} bfmt__tpl_capture_t;

typedef enum {
	BFMT__TPL_PLURAL,
	BFMT__TPL_ORDINAL,
	BFMT__TPL_SELECT,
} bfmt__tpl_kind_t;

static const char*
bfmt__tpl_argument(bfmt__tpl_t* tpl, const char* p, const bfmt__element_t* hash);

static inline bool
bfmt__tpl_is_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static inline bool
bfmt__tpl_is_digit(char c) {
	return c >= '0' && c <= '9';
}

static inline bool
bfmt__tpl_is_name_char(char c) {
	return c != '\0'
		&& c != '{' && c != '}' && c != ',' && c != '=' && c != '\'' && c != '#'
		&& !bfmt__tpl_is_space(c);
}

// Whether the apostrophe at `p` opens quoted text
static inline bool
bfmt__tpl_is_quote(const char* p) {
	return p[1] == '{' || p[1] == '}' || p[1] == '#';
}

static const char*
bfmt__tpl_skip_space(const char* p) {
	while (bfmt__tpl_is_space(*p)) { ++p; }
	return p;
}

static bool
bfmt__tpl_eq(const char* str, int len, const char* lit) {
	return strncmp(str, lit, (size_t)len) == 0 && lit[len] == '\0';
}

// Record the first error and return the end of the template so that every
// caller unwinds without doing further work.
// `resume` is the start of the text that has not been rendered.
static const char*
bfmt__tpl_fail(bfmt__tpl_t* tpl, const char* at, const char* resume) {
	if (tpl->error == NULL) {
		tpl->error = at;
		tpl->resume = resume;
	}
	return at + strlen(at);
}

static void
bfmt__tpl_write(bfmt__tpl_t* tpl, const char* begin, const char* end) {
	if (end > begin) {
		bfmt_write(tpl->out, begin, (int)(end - begin));
	}
}

static const bfmt__template_input_t*
bfmt__tpl_find_input(const bfmt__tpl_t* tpl, const char* name, int len) {
	for (const bfmt__template_input_t* itr = tpl->inputs; itr != NULL && itr->name != NULL; ++itr) {
		if (bfmt__tpl_eq(name, len, itr->name)) { return itr; }
	}
	return NULL;
}

// Read a builtin integer straight out of the element
static bool
bfmt__tpl_read_int(const bfmt__template_input_t* input, uint64_t* magnitude, bool* negative) {
	const void* value = input->element.value;
	long long sint;
	switch (input->kind) {
		case BFMT__VALUE_INT:    sint = *(const int*)value; break;
		case BFMT__VALUE_LONG:   sint = *(const long*)value; break;
		case BFMT__VALUE_LLONG:  sint = *(const long long*)value; break;
		case BFMT__VALUE_UINT:   *magnitude = *(const unsigned int*)value; return true;
		case BFMT__VALUE_ULONG:  *magnitude = *(const unsigned long*)value; return true;
		case BFMT__VALUE_ULLONG: *magnitude = *(const unsigned long long*)value; return true;
		default: return false;
	}

	*negative = sint < 0;
	*magnitude = sint < 0 ? 0 - (uint64_t)sint : (uint64_t)sint;
	return true;
}

static void
bfmt__tpl_capture_write(void* userdata, const char* str, int len) {
	bfmt__tpl_capture_t* capture = userdata;
	int space = (int)sizeof(capture->buf) - 1 - capture->len;
	if (len > space) { len = space; }
	memcpy(capture->buf + capture->len, str, (size_t)len);
	capture->len += len;
	capture->buf[capture->len] = '\0';
}

// Render an element into a buffer using its own formatter, for the values
// that can not be read directly: non-integer plural counts and non-string
// select keys.
static void
bfmt__tpl_capture(const bfmt__element_t* element, bfmt__tpl_capture_t* capture) {
	capture->len = 0;
	capture->buf[0] = '\0';
	bfmt_stream_t stream = { .write = bfmt__tpl_capture_write, .userdata = capture };
	bfmt_ctx_t ctx = { .stream = &stream };
	element->formatter(&ctx, element->value, element->options);
	if (ctx.bytes_buffered > 0) {
		stream.write(stream.userdata, ctx.buf, ctx.bytes_buffered);
	}
}

// Plural operands are defined over the visible digits: a value shown as "1.0"
// is not the same plural case as "1".
static void
bfmt__tpl_parse_number(const char* str, bfmt_plural_num_t* out, bool* negative) {
	bfmt_plural_num_t num = { 0 };
	const char* p = str;
	while (*p != '\0' && !bfmt__tpl_is_digit(*p)) {
		if (*p == '-') { *negative = true; }
		++p;
	}
	for (; bfmt__tpl_is_digit(*p); ++p) {
		num.i = num.i * 10 + (uint64_t)(*p - '0');
	}
	if (*p == '.') {
		for (++p; bfmt__tpl_is_digit(*p); ++p) {
			num.f = num.f * 10 + (uint64_t)(*p - '0');
			num.v += 1;
		}
	}
	num.t = num.f;
	num.w = num.v;
	while (num.w > 0 && num.t % 10 == 0) {
		num.t /= 10;
		num.w -= 1;
	}
	*out = num;
}

static bool
bfmt__tpl_category(const char* str, int len, bfmt_plural_t* category) {
	if (bfmt__tpl_eq(str, len, "zero")) { *category = BFMT_PLURAL_ZERO; return true; }
	if (bfmt__tpl_eq(str, len, "one"))  { *category = BFMT_PLURAL_ONE;  return true; }
	if (bfmt__tpl_eq(str, len, "two"))  { *category = BFMT_PLURAL_TWO;  return true; }
	if (bfmt__tpl_eq(str, len, "few"))  { *category = BFMT_PLURAL_FEW;  return true; }
	if (bfmt__tpl_eq(str, len, "many")) { *category = BFMT_PLURAL_MANY; return true; }
	return false;
}

// Skip message text until an unmatched '}' or the end of the template.
// Returns a pointer to that character.
static const char*
bfmt__tpl_skip(const char* p) {
	int depth = 0;
	for (;;) {
		switch (*p) {
			case '\0':
				return p;
			case '{':
				++depth;
				++p;
				break;
			case '}':
				if (depth == 0) { return p; }
				--depth;
				++p;
				break;
			case '\'':
				if (p[1] == '\'') {
					p += 2;
				} else if (bfmt__tpl_is_quote(p)) {
					for (++p; *p != '\0'; ++p) {
						if (*p != '\'') { continue; }
						++p;
						if (*p != '\'') { break; }  // Closing apostrophe
					}
				} else {
					++p;
				}
				break;
			default:
				++p;
				break;
		}
	}
}

// Render message text until an unmatched '}' or the end of the template.
// Returns a pointer to that character.
// `hash` is what `#` stands for, NULL outside of a plural case.
static const char*
bfmt__tpl_message(bfmt__tpl_t* tpl, const char* p, const bfmt__element_t* hash) {
	const char* run = p;  // Start of the pending literal run
	for (;;) {
		switch (*p) {
			case '\0':
			case '}':
				bfmt__tpl_write(tpl, run, p);
				return p;
			case '\'':
				if (p[1] == '\'') {  // '' is a literal apostrophe
					bfmt__tpl_write(tpl, run, p + 1);
					p += 2;
					run = p;
				} else if (bfmt__tpl_is_quote(p)) {
					bfmt__tpl_write(tpl, run, p);
					run = ++p;
					// Quoted text runs to the next lone apostrophe, or to the
					// end of the template if unterminated
					for (;;) {
						if (*p == '\0') {
							bfmt__tpl_write(tpl, run, p);
							return p;
						} else if (*p == '\'' && p[1] == '\'') {
							bfmt__tpl_write(tpl, run, p + 1);
							p += 2;
							run = p;
						} else if (*p == '\'') {
							bfmt__tpl_write(tpl, run, p);
							run = ++p;
							break;
						} else {
							++p;
						}
					}
				} else {  // A lone apostrophe is literal
					++p;
				}
				break;
			case '#':
				if (hash != NULL) {
					bfmt__tpl_write(tpl, run, p);
					hash->formatter(tpl->out, hash->value, hash->options);
					run = ++p;
				} else {
					++p;
				}
				break;
			case '{':
				bfmt__tpl_write(tpl, run, p);
				p = bfmt__tpl_argument(tpl, p, hash);
				run = p;
				break;
			default:
				++p;
				break;
		}
	}
}

// Render an argument starting at its '{'. Returns the position after the
// matching '}'.
static const char*
bfmt__tpl_argument(bfmt__tpl_t* tpl, const char* p, const bfmt__element_t* hash) {
	const char* start = p;

	p = bfmt__tpl_skip_space(p + 1);
	const char* name = p;
	while (bfmt__tpl_is_name_char(*p)) { ++p; }
	int name_len = (int)(p - name);
	if (name_len == 0) { return bfmt__tpl_fail(tpl, p, start); }

	const bfmt__template_input_t* input = bfmt__tpl_find_input(tpl, name, name_len);
	if (input == NULL) { return bfmt__tpl_fail(tpl, name, start); }

	p = bfmt__tpl_skip_space(p);
	if (*p == '}') {  // {name}
		input->element.formatter(tpl->out, input->element.value, input->element.options);
		return p + 1;
	}
	if (*p != ',') { return bfmt__tpl_fail(tpl, p, start); }

	p = bfmt__tpl_skip_space(p + 1);
	const char* type = p;
	while (bfmt__tpl_is_name_char(*p)) { ++p; }
	int type_len = (int)(p - type);
	bfmt__tpl_kind_t kind;
	if (bfmt__tpl_eq(type, type_len, "plural")) {
		kind = BFMT__TPL_PLURAL;
	} else if (bfmt__tpl_eq(type, type_len, "selectordinal")) {
		kind = BFMT__TPL_ORDINAL;
	} else if (bfmt__tpl_eq(type, type_len, "select")) {
		kind = BFMT__TPL_SELECT;
	} else {
		return bfmt__tpl_fail(tpl, type, start);
	}

	p = bfmt__tpl_skip_space(p);
	if (*p != ',') { return bfmt__tpl_fail(tpl, p, start); }
	p = bfmt__tpl_skip_space(p + 1);

	// Evaluate the selector. Integers and strings are read directly, anything
	// else has to go through its formatter first.
	bfmt__tpl_capture_t capture;
	bfmt_plural_num_t num = { 0 };
	bool negative = false;
	bfmt_plural_t category = BFMT_PLURAL_OTHER;
	const char* key = "";
	int key_len = 0;
	if (kind == BFMT__TPL_SELECT) {
		if (input->kind == BFMT__VALUE_STR) {
			key = *(const char* const*)input->element.value;
			if (key == NULL) { key = ""; }
			key_len = (int)strlen(key);
		} else {
			bfmt__tpl_capture(&input->element, &capture);
			key = capture.buf;
			key_len = capture.len;
		}
	} else {
		if (!bfmt__tpl_read_int(input, &num.i, &negative)) {
			bfmt__tpl_capture(&input->element, &capture);
			bfmt__tpl_parse_number(capture.buf, &num, &negative);
		}
		if (tpl->locale != NULL && tpl->locale->plural_rule != NULL) {
			category = tpl->locale->plural_rule(
				num,
				kind == BFMT__TPL_PLURAL ? BFMT_PLURAL_CARDINAL : BFMT_PLURAL_ORDINAL
			);
		}
	}

	// Skip over every case, remembering the bodies that match: cases may come
	// in any order and an explicit value beats a keyword.
	const char* explicit_body = NULL;
	const char* keyword_body = NULL;
	const char* other_body = NULL;
	for (;;) {
		p = bfmt__tpl_skip_space(p);
		if (*p == '}') { break; }
		if (*p == '\0') { return bfmt__tpl_fail(tpl, p, start); }

		bool is_explicit = false;
		bool is_keyword = false;
		bool is_other = false;
		if (*p == '=') {
			if (kind == BFMT__TPL_SELECT) { return bfmt__tpl_fail(tpl, p, start); }
			++p;
			if (!bfmt__tpl_is_digit(*p)) { return bfmt__tpl_fail(tpl, p, start); }
			uint64_t value = 0;
			for (; bfmt__tpl_is_digit(*p); ++p) {
				value = value * 10 + (uint64_t)(*p - '0');
			}
			is_explicit = !negative && num.f == 0 && num.i == value;
		} else {
			const char* selector = p;
			while (bfmt__tpl_is_name_char(*p)) { ++p; }
			int selector_len = (int)(p - selector);
			if (selector_len == 0) { return bfmt__tpl_fail(tpl, p, start); }

			is_other = bfmt__tpl_eq(selector, selector_len, "other");
			if (is_other) {
				// Nothing to match
			} else if (kind == BFMT__TPL_SELECT) {
				is_keyword = selector_len == key_len
					&& memcmp(selector, key, (size_t)selector_len) == 0;
			} else {
				bfmt_plural_t selector_category;
				is_keyword = bfmt__tpl_category(selector, selector_len, &selector_category)
					&& selector_category == category;
			}
		}

		p = bfmt__tpl_skip_space(p);
		if (*p != '{') { return bfmt__tpl_fail(tpl, p, start); }
		const char* body = p + 1;
		p = bfmt__tpl_skip(body);
		if (*p != '}') { return bfmt__tpl_fail(tpl, p, start); }
		++p;

		if (is_explicit && explicit_body == NULL) { explicit_body = body; }
		if (is_keyword && keyword_body == NULL) { keyword_body = body; }
		if (is_other && other_body == NULL) { other_body = body; }
	}
	if (other_body == NULL) { return bfmt__tpl_fail(tpl, p, start); }

	const char* body = explicit_body != NULL ? explicit_body
		: keyword_body != NULL ? keyword_body
		: other_body;
	// select keeps the enclosing '#'
	bfmt__tpl_message(tpl, body, kind == BFMT__TPL_SELECT ? hash : &input->element);
	if (tpl->error != NULL) { return p + strlen(p); }

	return p + 1;
}

void
bfmt__ptranslate_ctx(
	bfmt_ctx_t* ctx,
	bfmt_locale_t* locale,
	const char* context,
	const char* template,
	const bfmt__template_input_t* inputs
) {
	bfmt__tpl_t tpl = { .out = ctx, .locale = locale, .inputs = inputs };

	const char* text = NULL;
	if (locale != NULL && locale->translate != NULL) {
		text = locale->translate(locale->userdata, context, template);
	}
	if (text == NULL) { text = template; }

	const char* end = bfmt__tpl_message(&tpl, text, NULL);
	if (tpl.error == NULL && *end == '}') { bfmt__tpl_fail(&tpl, end, end); }  // Stray '}'

	if (tpl.error != NULL) {
		// Make the problem visible instead of silently truncating
		bfmt_write(ctx, tpl.resume, (int)strlen(tpl.resume));
		if (locale != NULL && locale->report_error != NULL) {
			locale->report_error(locale->userdata, context, template, text, (int)(tpl.error - text));
		}
	}
}

// }}}

// Configure nanoprintf
#define NANOPRINTF_VISIBILITY_STATIC
#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1

// nanoprintf {{{

/* nanoprintf @NPF_VERSION@: a tiny embeddable printf replacement written in C.
   https://github.com/charlesnicholson/nanoprintf
   charles.nicholson+nanoprintf@gmail.com
   dual-licensed under 0bsd and unlicense, take your pick. see eof for details. */

#ifndef NPF_H_INCLUDED
#define NPF_H_INCLUDED

#ifdef NANOPRINTF_CONFIG_FILE
#include NANOPRINTF_CONFIG_FILE
#endif

#include <stdarg.h>
#include <stddef.h>

typedef void (*npf_putc)(int c, void *ctx);

// Define this to fully sandbox nanoprintf inside of a translation unit.
#ifdef NANOPRINTF_VISIBILITY_STATIC
  #define NPF_VISIBILITY static
#else
  #define NPF_VISIBILITY extern
#endif

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
  #define NPF_PRINTF_ATTR(FORMAT_INDEX, VARGS_INDEX) \
    __attribute__((format(printf, FORMAT_INDEX, VARGS_INDEX)))
#else
  #define NPF_PRINTF_ATTR(FORMAT_INDEX, VARGS_INDEX)
#endif

#if defined(NANOPRINTF_USE_FLOAT_SINGLE_PRECISION) && \
    (NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1)
#define NPF_PRINTF_SP_ATTR NPF_PRINTF_ATTR(3, 0)
#define NPF_MAP_ARGS(...) NPF__MAP(NPF__WRAP, __VA_ARGS__)
typedef struct { float val; } npf_float_t;
#define npf_snprintf_  npf_snprintf_sp_
#define npf_pprintf_   npf_pprintf_sp_
#define npf_vsnprintf  npf_vsnprintf_sp
#define npf_vpprintf   npf_vpprintf_sp
#else
#define NPF_PRINTF_SP_ATTR NPF_PRINTF_ATTR(3, 4)
#define NPF_MAP_ARGS(...) __VA_ARGS__
#endif

#if !defined(__cplusplus) && \
    defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define NPF_RESTRICT restrict
#elif defined(__GNUC__) || defined(_MSC_VER)
#define NPF_RESTRICT __restrict
#else
#define NPF_RESTRICT
#endif

#ifdef __cplusplus
extern "C" {
#endif

NPF_VISIBILITY int npf_snprintf_(char * NPF_RESTRICT buffer,
                                 size_t bufsz,
                                 const char * NPF_RESTRICT format, ...)
                                 NPF_PRINTF_SP_ATTR;

NPF_VISIBILITY int npf_pprintf_(npf_putc pc,
                                void * NPF_RESTRICT pc_ctx,
                                char const * NPF_RESTRICT format, ...)
                                NPF_PRINTF_SP_ATTR;

// Public API

// The npf_ functions all return the number of bytes required to express the
// fully-formatted string, not including the null terminator character.
// The npf_ functions do not return negative values, since the lack of 'l' length
// modifier support makes encoding errors impossible.

#define npf_snprintf(buf, sz, ...) npf_snprintf_((buf), (sz), NPF_MAP_ARGS(__VA_ARGS__))
#define npf_pprintf(pc, ctx, ...) npf_pprintf_((pc), (ctx), NPF_MAP_ARGS(__VA_ARGS__))

NPF_VISIBILITY int npf_vsnprintf(char * NPF_RESTRICT buffer,
                                 size_t bufsz,
                                 char const * NPF_RESTRICT format,
                                 va_list vlist) NPF_PRINTF_ATTR(3, 0);

NPF_VISIBILITY int npf_vpprintf(npf_putc pc,
                                void * NPF_RESTRICT pc_ctx,
                                char const * NPF_RESTRICT format,
                                va_list vlist) NPF_PRINTF_ATTR(3, 0);

#ifdef __cplusplus
}
#endif

#endif // NPF_H_INCLUDED

/* The implementation of nanoprintf begins here, to be compiled only if
   NANOPRINTF_IMPLEMENTATION is defined. In a multi-file library what follows would
   be nanoprintf.c. */

#ifdef NANOPRINTF_IMPLEMENTATION

#ifndef NPF_IMPLEMENTATION_INCLUDED
#define NPF_IMPLEMENTATION_INCLUDED

#include <limits.h>
#include <stdint.h>

// Pick reasonable defaults if nothing's been configured.
#if !defined(NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS) && \
    !defined(NANOPRINTF_USE_ALT_FORM_FLAG) && \
    !defined(NANOPRINTF_USE_FLOAT_SINGLE_PRECISION)
  #define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
  #define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
  #define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
  #define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
  #define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
  #define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
  #define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
  #define NANOPRINTF_USE_ALT_FORM_FLAG 1
  #define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0
#endif

// Single-precision mode defaults to off unless explicitly enabled.
#ifndef NANOPRINTF_USE_FLOAT_SINGLE_PRECISION
  #define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0
#endif

// Optional flags, default to 0 if not explicitly configured.
#ifndef NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER
  #define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER 0
#endif
#ifndef NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER
  #define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER 0
#endif
#ifndef NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER
  #define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER 0
#endif
#ifndef NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS
  #define NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS 0
#endif

// 'e' and 'g' share a conversion function; 'g' selects between 'e' and 'f' output.
#if (NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1) || \
    (NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1)
  #define NPF_USE_SCI 1
#else
  #define NPF_USE_SCI 0
#endif

// Optional flag, defaults to 0 if not explicitly configured. Extracts digits
// without integer division; smaller on cores without a hardware divider.
#ifndef NANOPRINTF_USE_DIVISION_FREE_CONVERSION
  #define NANOPRINTF_USE_DIVISION_FREE_CONVERSION 0
#endif

// If anything's been configured, everything must be configured.
#ifndef NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif
#ifndef NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS
  #error NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif

// Ensure flags are compatible.
#if (NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1) && \
    (NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 0)
  #error Float format specifiers must be enabled if float hex support is enabled.
#endif

#if (NPF_USE_SCI == 1) && (NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 0)
  #error Float format specifiers must be enabled if float sci/shortest support is enabled.
#endif

#if (NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1) && \
    (NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 0)
  #error Single precision requires float format specifiers to be enabled.
#endif

// 'w8' and 'w16' resolve to the 'hh' and 'h' length modifiers.
#if (NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS == 1) && \
    (NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 0)
  #error Small format specifiers must be enabled if fixed-width support is enabled.
#endif

/* C23 requires 'wN' to cover every exact-width and minimum-width type stdint.h
   defines, not only the four widths it requires stdint.h to define. Those four
   always name a typedef for char / short / int / long / long long, which is what
   lets 'wN' resolve to a length modifier that already exists; a width no standard
   type has would instead need the promoted value masked at runtime. Nothing
   defines such a type today, so rather than carry that code, catch the target
   that would need it. */
#if NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS == 1
  #if defined(INT24_MAX) || defined(INT_LEAST24_MAX) || \
      defined(INT40_MAX) || defined(INT_LEAST40_MAX) || \
      defined(INT48_MAX) || defined(INT_LEAST48_MAX) || \
      defined(INT128_MAX) || defined(INT_LEAST128_MAX)
    #error Fixed-width specifiers cannot convert to the other stdint widths here.
  #endif
#endif

#if NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1
  #if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
    #error Single-precision float mode requires /Zc:preprocessor on MSVC.
  #endif
  #ifdef __cplusplus
    #if __cplusplus < 201103L && !defined(_MSC_VER)
      #error Single-precision float wrapping requires C++11 or later.
    #endif
  #else
    #if !(defined(__GNUC__) || defined(__clang__))
      #if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
        #error Single-precision float wrapping requires C11 or later (or GCC/Clang).
      #endif
    #endif
  #endif
#endif

// The conversion buffer must fit at least UINT64_MAX in octal format with the leading '0'.
// When floats are enabled, a larger buffer is needed for values like FLT_MAX / DBL_MAX.
#ifndef NANOPRINTF_CONVERSION_BUFFER_SIZE
  #if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
    #define NANOPRINTF_CONVERSION_BUFFER_SIZE  64
  #else
    #define NANOPRINTF_CONVERSION_BUFFER_SIZE  23
  #endif
#endif
#if NANOPRINTF_CONVERSION_BUFFER_SIZE < 23
  #error The size of the conversion buffer must be at least 23 bytes.
#endif

/* The macro is the user's, so it may be an expression and may be unsigned; every
   in-code use goes through this int-typed, parenthesized alias instead. Unsigned
   would turn the signed comparisons against it into unsigned ones. */
#define NPF_CBUF ((int)(NANOPRINTF_CONVERSION_BUFFER_SIZE))

/* Ceiling for a field width or precision, whether from the format string or a
   star argument. Values are int-typed and user-supplied, so an unbounded one
   overflows on the way to the output-length arithmetic; INT_MIN cannot even be
   negated. Any cap avoids that, and this one clears the 4095 that C11 5.2.4.1
   requires an implementation to accept. A width and a precision both land in the
   same length, so the cap also has to leave twice itself plus a conversion buffer
   inside int: fine at 0xFF00 for a 32-bit int, so 0x2000 where int is 16 bits.
   0xFF00 rather than a rounder number because it is one of the immediates Thumb-2
   can fold into a compare. */
#if INT_MAX < 65280
  #define NPF_FMT_NUM_MAX 8192
#elif NANOPRINTF_CONVERSION_BUFFER_SIZE > 65280
  #define NPF_FMT_NUM_MAX NPF_CBUF
#else
  #define NPF_FMT_NUM_MAX 65280
#endif

/* True once the accumulator below is too large for another decimal digit to fit in
   int: five bits below the width, so 10n+9 cannot reach INT_MAX. The 27 covers int
   at 32 bits and above, where the digits past it are discarded anyway. A shift is
   the cheapest spelling where one instruction can shift by 27; a 16-bit int is on
   a machine without a barrel shifter, so there it is a mask instead. */
#if (INT_MAX >> 27) != 0
  #define NPF_FMT_NUM_BIG(n) ((n) >> 27)
#else
  #define NPF_FMT_NUM_BIG(n) ((n) & ~0x7FF)
#endif

/* Precision the float conversions run with. When precision specifiers are
   compiled out, the conversion default is a constant the compiler folds. */
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
  #define NPF_DEC_PREC(spec) ((spec)->prec)
  #define NPF_HEX_PREC(spec) ((spec)->prec)
#else
  #define NPF_DEC_PREC(spec) 6
  #define NPF_HEX_PREC(spec) ((NPF_DOUBLE_MAN_BITS + 3) / 4)
#endif

// intmax_t / uintmax_t require stdint from c99 / c++11
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
  #ifndef _MSC_VER
    #ifdef __cplusplus
      #if __cplusplus < 201103L
        #error large format specifier support requires C++11 or later.
      #endif
    #else
      #if __STDC_VERSION__ < 199409L
        #error nanoprintf requires C99 or later.
      #endif
    #endif
  #endif
#endif

// Figure out if we can disable warnings with pragmas.
#ifdef __clang__
  #define NPF_CLANG 1
  #define NPF_GCC_PAST_4_6 0
#else
  #define NPF_CLANG 0
  #if defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 6)))
    #define NPF_GCC_PAST_4_6 1
  #else
    #define NPF_GCC_PAST_4_6 0
  #endif
#endif

#if NPF_CLANG || NPF_GCC_PAST_4_6
  #define NPF_HAVE_GCC_WARNING_PRAGMAS 1
#else
  #define NPF_HAVE_GCC_WARNING_PRAGMAS 0
#endif

#if NPF_HAVE_GCC_WARNING_PRAGMAS
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpragmas"
  #pragma GCC diagnostic ignored "-Wfloat-equal"
  #pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
  #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
  #pragma GCC diagnostic ignored "-Wpadded"
  #pragma GCC diagnostic ignored "-Wpedantic"
  #pragma GCC diagnostic ignored "-Wunused-function"

  #ifdef __cplusplus
    #pragma GCC diagnostic ignored "-Wold-style-cast"
  #endif

  #if NPF_CLANG
    #pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"
    #pragma GCC diagnostic ignored "-Wcovered-switch-default"
    #pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
    #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    #ifndef __APPLE__
      #pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
    #endif
  #elif NPF_GCC_PAST_4_6
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  #endif
#endif

#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4619) // there is no warning number 'number'
  // C4619 has to be disabled first!
  #pragma warning(disable:4127) // conditional expression is constant
  #pragma warning(disable:4505) // unreferenced local function has been removed
  #pragma warning(disable:4514) // unreferenced inline function has been removed
  #pragma warning(disable:4701) // potentially uninitialized local variable used
  #pragma warning(disable:4706) // assignment within conditional expression
  #pragma warning(disable:4710) // function not inlined
  #pragma warning(disable:4711) // function selected for inline expansion
  #pragma warning(disable:4820) // padding added after struct member
  #pragma warning(disable:5039) // potentially throwing function passed to extern C function
  #pragma warning(disable:5045) // compiler will insert Spectre mitigation for memory load
  #pragma warning(disable:5262) // implicit switch fall-through
  #pragma warning(disable:26812) // enum type is unscoped
#endif

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
  #define NPF_NOINLINE __attribute__((noinline))
  #define NPF_FORCE_INLINE inline __attribute__((always_inline))

  #define NPF_MIN(X, Y) ({ \
    __typeof__(X) const x = (X); __typeof__(Y) const y = (Y); x <= y ? x : y; })
  #define NPF_MAX(X, Y) ({ \
    __typeof__(X) const x = (X); __typeof__(Y) const y = (Y); x >= y ? x : y; })
#else
  #if defined(_MSC_VER)
    #define NPF_NOINLINE __declspec(noinline)
    #define NPF_FORCE_INLINE inline __forceinline
  #else
    #define NPF_NOINLINE
    #define NPF_FORCE_INLINE
  #endif

  #define NPF_MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
  #define NPF_MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#endif

#if (NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1) || \
    (NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1)
enum {
  NPF_FMT_SPEC_OPT_NONE,
  NPF_FMT_SPEC_OPT_LITERAL,
  NPF_FMT_SPEC_OPT_STAR,
};
#endif

enum {
  NPF_FMT_SPEC_LEN_MOD_NONE,
#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 1
  NPF_FMT_SPEC_LEN_MOD_SHORT,       // 'h'
  NPF_FMT_SPEC_LEN_MOD_CHAR,        // 'hh'
#endif
  NPF_FMT_SPEC_LEN_MOD_LONG,        // 'l'
  NPF_FMT_SPEC_LEN_MOD_LONG_DOUBLE, // 'L'
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
  NPF_FMT_SPEC_LEN_MOD_LARGE_LONG_LONG, // 'll'
  NPF_FMT_SPEC_LEN_MOD_LARGE_INTMAX,    // 'j'
  NPF_FMT_SPEC_LEN_MOD_LARGE_SIZET,     // 'z'
  NPF_FMT_SPEC_LEN_MOD_LARGE_PTRDIFFT,  // 't'
#endif
};

#if NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS == 1
/* 'wN' names int_leastN_t and 'wfN' names int_fastN_t, and each of those is a
   typedef for a type that some length modifier already carries. Resolving N to
   that modifier at parse time is the entire feature: extraction, conversion and
   writeback then run on the 'hh' / 'h' / 'l' / 'll' paths unchanged. */
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
  #define NPF_LM_WIDEST NPF_FMT_SPEC_LEN_MOD_LARGE_LONG_LONG
  #define NPF_W_BITS_MAX 64
#elif (INT_LEAST64_MAX <= LONG_MAX) && (INT_FAST64_MAX <= LONG_MAX)
  #define NPF_LM_WIDEST NPF_FMT_SPEC_LEN_MOD_LONG
  #define NPF_W_BITS_MAX 64
#else
  // Nothing here carries a 'long long', so 'w64' and 'wf64' don't parse.
  #define NPF_LM_WIDEST NPF_FMT_SPEC_LEN_MOD_LONG
  #define NPF_W_BITS_MAX 32
#endif

// The narrowest length modifier whose type holds MAX: the one stdint.h picked.
#define NPF_LM_OF(MAX) (uint8_t)( \
  ((MAX) <= SCHAR_MAX) ? NPF_FMT_SPEC_LEN_MOD_CHAR : \
  ((MAX) <= SHRT_MAX)  ? NPF_FMT_SPEC_LEN_MOD_SHORT : \
  ((MAX) <= INT_MAX)   ? NPF_FMT_SPEC_LEN_MOD_NONE : \
  ((MAX) <= LONG_MAX)  ? NPF_FMT_SPEC_LEN_MOD_LONG : NPF_LM_WIDEST)

/* The modifier for one width, either family. The two coincide unless the
   platform's fastest type of that width is wider than its narrowest, so this
   usually folds to a constant and the parse carries no mapping table at all. */
#define NPF_LM_OF_W(FAST, N) \
  ((FAST) ? NPF_LM_OF(INT_FAST##N##_MAX) : NPF_LM_OF(INT_LEAST##N##_MAX))
#endif

enum {
  NPF_FMT_SPEC_CONV_NONE,
  NPF_FMT_SPEC_CONV_PERCENT,      // '%'
  NPF_FMT_SPEC_CONV_CHAR,         // 'c'
  NPF_FMT_SPEC_CONV_STRING,       // 's'
  NPF_FMT_SPEC_CONV_SIGNED_INT,   // 'i', 'd'
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
  NPF_FMT_SPEC_CONV_BINARY,       // 'b'
#endif
  NPF_FMT_SPEC_CONV_OCTAL,        // 'o'
  NPF_FMT_SPEC_CONV_HEX_INT,      // 'x', 'X'
  NPF_FMT_SPEC_CONV_UNSIGNED_INT, // 'u'
  NPF_FMT_SPEC_CONV_POINTER,      // 'p'
#if NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS == 1
  NPF_FMT_SPEC_CONV_WRITEBACK,    // 'n'
#endif
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
  NPF_FMT_SPEC_CONV_FLOAT_DEC,      // 'f', 'F'
#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1
  NPF_FMT_SPEC_CONV_FLOAT_HEX,      // 'a', 'A'
#endif
#if NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1
  NPF_FMT_SPEC_CONV_FLOAT_SCI,      // 'e', 'E'
#endif
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
  NPF_FMT_SPEC_CONV_FLOAT_SHORTEST, // 'g', 'G'
#endif
#endif
};

// The lowest-numbered of the sci-family convs, whichever of them is compiled in.
#if NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1
  #define NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST NPF_FMT_SPEC_CONV_FLOAT_SCI
#elif NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
  #define NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST NPF_FMT_SPEC_CONV_FLOAT_SHORTEST
#endif

// Assert range order/comparisons to enforce C standard ordering
#define NPF_CONV_ORDER_ASSERT(NAME, COND) \
  typedef char npf_assert_##NAME[(COND) ? 1 : -1]
NPF_CONV_ORDER_ASSERT(text_convs_before_numeric,
  (NPF_FMT_SPEC_CONV_PERCENT < NPF_FMT_SPEC_CONV_SIGNED_INT) &&
  (NPF_FMT_SPEC_CONV_CHAR < NPF_FMT_SPEC_CONV_SIGNED_INT) &&
  (NPF_FMT_SPEC_CONV_STRING < NPF_FMT_SPEC_CONV_SIGNED_INT));
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
NPF_CONV_ORDER_ASSERT(int_convs_contiguous,
  NPF_FMT_SPEC_CONV_UNSIGNED_INT == NPF_FMT_SPEC_CONV_SIGNED_INT + 4);
#else
NPF_CONV_ORDER_ASSERT(int_convs_contiguous,
  NPF_FMT_SPEC_CONV_UNSIGNED_INT == NPF_FMT_SPEC_CONV_SIGNED_INT + 3);
#endif
NPF_CONV_ORDER_ASSERT(pointer_after_int_convs,
  NPF_FMT_SPEC_CONV_POINTER > NPF_FMT_SPEC_CONV_UNSIGNED_INT);
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
#if NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS == 1
NPF_CONV_ORDER_ASSERT(float_convs_last,
  NPF_FMT_SPEC_CONV_FLOAT_DEC > NPF_FMT_SPEC_CONV_WRITEBACK);
#else
NPF_CONV_ORDER_ASSERT(float_convs_last,
  NPF_FMT_SPEC_CONV_FLOAT_DEC > NPF_FMT_SPEC_CONV_POINTER);
#endif
// 'e'/'g' are dispatched by a single >= FLOAT_SCI_FIRST range test, so they must be
// the last float convs; 'a' is dispatched by equality and sits between them and 'f'.
#if NPF_USE_SCI == 1
NPF_CONV_ORDER_ASSERT(sci_convs_after_other_floats,
  NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST > NPF_FMT_SPEC_CONV_FLOAT_DEC);
#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1
NPF_CONV_ORDER_ASSERT(hex_conv_before_sci_convs,
  NPF_FMT_SPEC_CONV_FLOAT_HEX < NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST);
#endif
#if (NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1) && \
    (NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1)
NPF_CONV_ORDER_ASSERT(sci_convs_contiguous,
  NPF_FMT_SPEC_CONV_FLOAT_SHORTEST == NPF_FMT_SPEC_CONV_FLOAT_SCI + 1);
#endif
#endif
#endif
#undef NPF_CONV_ORDER_ASSERT

typedef struct npf_format_spec {
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
  int field_width;
#endif
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
  int prec;
  uint8_t prec_opt;
#endif
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
  uint8_t field_width_opt;
  char left_justified;   // '-'
  char leading_zero_pad; // '0'
#endif
  char prepend;          // ' ' or '+'
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
  char alt_form;         // '#'
#endif
  char case_adjust;      // 'a' - 'A' , or 0 (must be non-negative to work)
  uint8_t length_modifier;
  uint8_t conv_spec;
} npf_format_spec_t;

#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
  typedef intmax_t npf_int_t;
  typedef uintmax_t npf_uint_t;
  #define NPF_UINT_IS_WIDE 1 // uintmax_t is at least 64 bits
#elif ULONG_MAX > UINTPTR_MAX
  typedef long npf_int_t;
  typedef unsigned long npf_uint_t;
  #define NPF_UINT_IS_WIDE (ULONG_MAX > 0xFFFFFFFFu)
#else
  typedef intptr_t npf_int_t;
  typedef uintptr_t npf_uint_t;
  #define NPF_UINT_IS_WIDE (UINTPTR_MAX > 0xFFFFFFFFu)
#endif

typedef struct npf_bufputc_ctx {
  char *dst;       // moving cursor; advances on each write while len > 0.
  size_t len;      // remaining capacity; decrements on each successful write.
} npf_bufputc_ctx_t;

#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
  typedef char npf_size_is_ptrdiff[(sizeof(size_t) == sizeof(ptrdiff_t)) ? 1 : -1];
  typedef ptrdiff_t npf_ssize_t;
  typedef size_t npf_uptrdiff_t;
#endif

#ifdef _MSC_VER
  #include <intrin.h>
#endif

#if (NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1) || \
    (NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1)
/* Consumes a decimal run into *out and returns the cursor. Nothing in the format
   string bounds the digit count, and an int that wraps here would hand the
   conversions a negative width or precision, so the accumulator stops growing
   once another digit could carry it out of int. npf_vpprintf applies
   NPF_FMT_NUM_MAX after this, so the digits past the stop do not have to survive.
   Testing the top bit beats testing against the ceiling itself: no constant to
   materialize, and this is the one site the caller inlines twice. */
static char const *npf_fmt_num(char const *cur, int *out) {
  int n = 0;
  while ((unsigned)(*cur - '0') < 10u) {
    if (!NPF_FMT_NUM_BIG(n)) { n = (n * 10) + (*cur - '0'); }
    ++cur;
  }
  *out = n;
  return cur;
}
#endif

// Returns a pointer one past the last consumed character on success, null on
// failure. A pointer return inlines into npf_vpprintf with smaller code than
// a length return (the caller continues from the returned cursor directly).
static char const *npf_parse_format_spec_end(char const *format,
                                             npf_format_spec_t *out_spec) {
  char const *cur = format;

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
  out_spec->left_justified = 0;
  out_spec->leading_zero_pad = 0;
#endif
  out_spec->prepend = 0;
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
  out_spec->alt_form = 0;
#endif

  for (;;) { // cur points at the leading '%' character
    switch (*++cur) { // Optional flags; '\0' exits via default.
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
      case '-': out_spec->left_justified = '-'; continue;
      case '0': out_spec->leading_zero_pad = 1; continue;
#endif
      case '+':
      case ' ': if (out_spec->prepend != '+') { out_spec->prepend = *cur; } continue;
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
      case '#': out_spec->alt_form = '#'; continue;
#endif
      default: break;
    }
    break;
  }

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
  /* Only STAR is ever read back for a field width: a literal one and an absent one
     both mean "use field_width as-is", which npf_fmt_num sets to 0 when the digit
     run is empty. So this stays two-state, unlike prec_opt, whose NONE selects the
     conversion's default precision and so must be told apart from a literal. */
  out_spec->field_width_opt = NPF_FMT_SPEC_OPT_NONE;
  if (*cur == '*') {
    out_spec->field_width_opt = NPF_FMT_SPEC_OPT_STAR;
    ++cur;
  } else {
    cur = npf_fmt_num(cur, &out_spec->field_width);
  }
#endif

#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
  out_spec->prec = 0;
  out_spec->prec_opt = NPF_FMT_SPEC_OPT_NONE;
  if (*cur == '.') {
    ++cur;
    if (*cur == '*') {
      out_spec->prec_opt = NPF_FMT_SPEC_OPT_STAR;
      ++cur;
    } else {
      if (*cur == '-') {
        ++cur;
      } else {
        out_spec->prec_opt = NPF_FMT_SPEC_OPT_LITERAL;
      }
      cur = npf_fmt_num(cur, &out_spec->prec);
    }
  }
#endif

  out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_NONE;
#if NANOPRINTF_USE_FIXED_WIDTH_FORMAT_SPECIFIERS == 1
  if (*cur == 'w') {
    ++cur;
    uint_fast8_t fast = 0;
    if (*cur == 'f') { fast = 1; ++cur; }
    /* The widths stdint.h is required to define are the only ones supported,
       and the first digit tells them apart; a second digit, where the width
       has one, only has to be confirmed. Nothing here needs the value, so no
       arithmetic is spent building one. */
    char const c = *cur++;
    char d2 = 0;
    if (c == '1') { out_spec->length_modifier = NPF_LM_OF_W(fast, 16); d2 = '6'; }
    else if (c == '3') { out_spec->length_modifier = NPF_LM_OF_W(fast, 32); d2 = '2'; }
#if NPF_W_BITS_MAX == 64
    else if (c == '6') { out_spec->length_modifier = NPF_LM_OF_W(fast, 64); d2 = '4'; }
#endif
    else if (c == '8') { out_spec->length_modifier = NPF_LM_OF_W(fast, 8); }
    else { return NULL; }
    if (d2) {
      if (*cur != d2) { return NULL; }
      ++cur;
    }
  } else
#endif
  switch (*cur++) { // Length modifier
#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 1
    case 'h':
      out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_SHORT;
      if (*cur == 'h') {
        out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_CHAR;
        ++cur;
      }
      break;
#endif
    case 'l':
      /* 'l' makes 'c' and 's' wide, and converting those needs wcrtomb. Printing
         the argument's bytes as narrow characters would look right and not be,
         so "%lc" and "%ls" fail to parse like any other unsupported conversion.
         OR-ing in 0x30 maps exactly 'c', 's', 'C', and 'S' to 's'. */
      if ((*cur | 0x30) == 's') { return NULL; }
      out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LONG;
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
      if (*cur == 'l') {
        out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LARGE_LONG_LONG;
        ++cur;
      }
#endif
      break;
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
    case 'L': out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LONG_DOUBLE; break;
#endif
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
    case 'j': out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LARGE_INTMAX; break;
    case 'z': out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LARGE_SIZET; break;
    case 't': out_spec->length_modifier = NPF_FMT_SPEC_LEN_MOD_LARGE_PTRDIFFT; break;
#endif
    default: --cur; break;
  }

  // Conversion specifier. Look up the conv_spec from a 24-byte table indexed by
  // (lowercased letter - 'a'). '%' is handled out-of-line since it's the only
  // non-letter conversion. case_adjust gets bit 5 of the original char.
  char const c = *cur++;
  uint_fast8_t cs;
  if (c == '%') {
    cs = NPF_FMT_SPEC_CONV_PERCENT;
  } else {
    static const uint8_t lookup[24] = {
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1 && NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1
      NPF_FMT_SPEC_CONV_FLOAT_HEX,      // 'a'
#else
      0,                                 // 'a'
#endif
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
      NPF_FMT_SPEC_CONV_BINARY,          // 'b'
#else
      0,                                 // 'b'
#endif
      NPF_FMT_SPEC_CONV_CHAR,            // 'c'
      NPF_FMT_SPEC_CONV_SIGNED_INT,      // 'd'
      // A conv whose feature is compiled out maps to 0, fails to parse, and is
      // emitted verbatim. That's the signal that the build is misconfigured.
#if NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1
      NPF_FMT_SPEC_CONV_FLOAT_SCI,       // 'e'
#else
      0,                                 // 'e'
#endif
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
      NPF_FMT_SPEC_CONV_FLOAT_DEC,       // 'f'
#else
      0,                                 // 'f'
#endif
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
      NPF_FMT_SPEC_CONV_FLOAT_SHORTEST,  // 'g'
#else
      0,                                 // 'g'
#endif
      0,                                 // 'h' (length modifier)
      NPF_FMT_SPEC_CONV_SIGNED_INT,      // 'i'
      0, 0, 0, 0,                        // 'j', 'k', 'l', 'm'
#if NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS == 1
      NPF_FMT_SPEC_CONV_WRITEBACK,       // 'n'
#else
      0,                                 // 'n'
#endif
      NPF_FMT_SPEC_CONV_OCTAL,           // 'o'
      NPF_FMT_SPEC_CONV_POINTER,         // 'p'
      0, 0,                              // 'q', 'r'
      NPF_FMT_SPEC_CONV_STRING,          // 's'
      0,                                 // 't'
      NPF_FMT_SPEC_CONV_UNSIGNED_INT,    // 'u'
      0, 0,                              // 'v', 'w'
      NPF_FMT_SPEC_CONV_HEX_INT,         // 'x'
    };
    unsigned const idx = (unsigned)((c | 32) - 'a');
    if (idx >= sizeof(lookup) || !(cs = lookup[idx])) { return NULL; }
  }
  out_spec->conv_spec = (uint8_t)cs;
  out_spec->case_adjust = (char)(c & 32); // 32 for lowercase, 0 for uppercase

  return cur;
}

// Length-returning facade over npf_parse_format_spec_end; 0 means failure.
static NPF_FORCE_INLINE int npf_parse_format_spec(char const *format,
                                                  npf_format_spec_t *out_spec) {
  char const *const end = npf_parse_format_spec_end(format, out_spec);
  return end ? (int)(end - format) : 0;
}

#if NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1
// Calculate div by 10 justing a shift and mask to avoid using hw div operands
static NPF_NOINLINE uint32_t npf_div10(uint32_t n) {
  uint32_t q = (n >> 1) + (n >> 2);
  q += q >> 4;
  q += q >> 8;
  q += q >> 16;
  q >>= 3;
  return q + ((n - (q * 10u) + 6u) >> 4);
}
#endif

static NPF_NOINLINE char *npf_utoa_rev_end(
    npf_uint_t val, char *buf, uint_fast8_t base, char case_adj) {
#if (NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1) || \
    ((NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1) && NPF_UINT_IS_WIDE)
  // Use shift and subtract here to avoid hw div operation
  while (val > 0xFFFFFFFFu) {
    npf_uint_t q = 0, r = 0;
    for (int i = (int)(sizeof(val) * 8) - 1; i >= 0; --i) {
      r = (r << 1) | ((val >> i) & 1);
      if (r >= (npf_uint_t)base) { r -= base; q |= (npf_uint_t)1 << i; }
    }
    int_fast8_t const d = (int_fast8_t)r;
    *buf++ = (char)(((d < 10) ? '0' : ('A' - 10 + case_adj)) + d);
    val = q;
  }
  uint32_t v32 = (uint32_t)val;
#else
  npf_uint_t v32 = val;
#endif
  do {
#if NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1
    int_fast8_t d;
    if (base == 10u) {
      uint32_t const q = npf_div10((uint32_t)v32);
      d = (int_fast8_t)((uint32_t)v32 - (q * 10u));
      v32 = q;
    } else { // base 8 or 16: shift and mask
      d = (int_fast8_t)(v32 & (base - 1u));
      v32 >>= (base + 16u) >> 3; // 8 -> 3, 16 -> 4
    }
#else
    int_fast8_t const d = (int_fast8_t)(v32 % base);
    v32 /= base;
#endif
    *buf++ = (char)(((d < 10) ? '0' : ('A' - 10 + case_adj)) + d);
  } while (v32);
  return buf;
}

// Length-returning facade over npf_utoa_rev_end.
static NPF_FORCE_INLINE int npf_utoa_rev(
    npf_uint_t val, char *buf, uint_fast8_t base, char case_adj) {
  return (int)(npf_utoa_rev_end(val, buf, base, case_adj) - buf);
}

#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1

#include <float.h>

#if NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1
  typedef float npf_real_t;
  #define NPF_REAL_MANT_DIG FLT_MANT_DIG
  #define NPF_REAL_MAX_EXP  FLT_MAX_EXP
#else
  typedef double npf_real_t;
  #define NPF_REAL_MANT_DIG DBL_MANT_DIG
  #define NPF_REAL_MAX_EXP  DBL_MAX_EXP
#endif

#if (NPF_REAL_MANT_DIG <= 11) && (NPF_REAL_MAX_EXP <= 16)
  typedef uint_fast16_t npf_real_bin_t;
  typedef int_fast8_t npf_ftoa_exp_t;
#elif (NPF_REAL_MANT_DIG <= 24) && (NPF_REAL_MAX_EXP <= 128)
  typedef uint_fast32_t npf_real_bin_t;
  typedef int_fast16_t npf_ftoa_exp_t;
#elif (NPF_REAL_MANT_DIG <= 53) && (NPF_REAL_MAX_EXP <= 1024)
  typedef uint_fast64_t npf_real_bin_t;
  typedef int_fast16_t npf_ftoa_exp_t;
#else
  #error Unsupported width of the real type.
#endif

// The floating point conversion code works with an unsigned integer type of any size.
#ifndef NANOPRINTF_CONVERSION_FLOAT_TYPE
  #define NANOPRINTF_CONVERSION_FLOAT_TYPE unsigned int
#endif
typedef NANOPRINTF_CONVERSION_FLOAT_TYPE npf_ftoa_man_t;

#if (NANOPRINTF_CONVERSION_BUFFER_SIZE <= UINT_FAST8_MAX) && (UINT_FAST8_MAX <= INT_MAX)
  typedef uint_fast8_t npf_ftoa_dec_t;
#else
  typedef int npf_ftoa_dec_t;
#endif

enum {
  NPF_REAL_EXP_MASK = NPF_REAL_MAX_EXP * 2 - 1,
  NPF_REAL_EXP_BIAS = NPF_REAL_MAX_EXP - 1,
  NPF_REAL_MAN_BITS = NPF_REAL_MANT_DIG - 1,
  NPF_REAL_BIN_BITS = sizeof(npf_real_bin_t) * CHAR_BIT,
  NPF_REAL_SIGN_POS = sizeof(npf_real_t) * CHAR_BIT - 1,
  NPF_FTOA_MAN_BITS   = sizeof(npf_ftoa_man_t) * CHAR_BIT,
  NPF_FTOA_SHIFT_BITS =
    ((NPF_FTOA_MAN_BITS < NPF_REAL_MANT_DIG) ? NPF_FTOA_MAN_BITS : NPF_REAL_MANT_DIG) - 1
};

/* Generally, floating-point conversion implementations use
   grisu2 (https://bit.ly/2JgMggX) and ryu (https://bit.ly/2RLXSg0) algorithms,
   which are mathematically exact and fast, but require large lookup tables.

   This implementation was inspired by Wojciech Muła's (zdjęcia@garnek.pl)
   algorithm (http://0x80.pl/notesen/2015-12-29-float-to-string.html) and
   extended further by adding dynamic scaling and configurable integer width by
   Oskars Rubenis (https://github.com/Okarss). */

static NPF_FORCE_INLINE npf_real_bin_t npf_real_to_int_rep(npf_real_t f) {
  // Union-cast is UB pre-C11 and in all C++; the compiler optimizes the code below.
  npf_real_bin_t bin = 0;
  char const *src = (char const *)&f;
  char *dst = (char *)&bin;
  for (uint_fast8_t i = 0; i < sizeof(f); ++i) { dst[i] = src[i]; }
  return bin;
}

#if NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1
// Variable shifts of 64-bit values call sw helpers on archs
// without 64-bit shifters. Perfer 1 bit shits to keep in 32 bit word spce.
static NPF_FORCE_INLINE npf_real_bin_t npf_bin_shr(npf_real_bin_t v, int_fast8_t s) {
  if (sizeof(v) > sizeof(uint32_t)) { while (s--) { v >>= 1; } return v; }
  return (npf_real_bin_t)(v >> s);
}
static NPF_FORCE_INLINE npf_real_bin_t npf_bin_shl(npf_real_bin_t v, int_fast8_t s) {
  if (sizeof(v) > sizeof(uint32_t)) { while (s--) { v <<= 1; } return v; }
  return (npf_real_bin_t)(v << s);
}
  #define NPF_BIN_SHR(V, S) npf_bin_shr((V), (int_fast8_t)(S))
  #define NPF_BIN_SHL(V, S) npf_bin_shl((V), (int_fast8_t)(S))
#else
  #define NPF_BIN_SHR(V, S) ((npf_real_bin_t)((V) >> (S)))
  #define NPF_BIN_SHL(V, S) ((npf_real_bin_t)((V) << (S)))
#endif

/* Only one of the two float conversions is compiled at a time. npf_ftoa_rev knows
   where the decimal point goes before it starts, so it fuses digit generation with
   placement; that is the most compact way to do 'f' by itself, and it is what gets
   compiled when 'f' is all that is enabled. npf_etoa_rev cannot fuse them, because
   'e' does not know the decimal exponent until the digits are generated and
   rounded, so it generates digits plus an exponent and lays out afterwards. When
   'e' or 'g' is enabled, 'f' uses that generator too rather than adding a second
   one, which is why npf_ftoa_rev is compiled out in those configurations.

   The two are held to each other by tests/unit_f_paths.cc. */

// Emits a reversed special into buf: 0 -> "NAN", 4 -> "INF", 8 -> "ERR". Returns the
// negated length, the caller's signal that the payload is text and not a number.
static int npf_ftoa_special(char *buf, char case_adj, uint_fast8_t off) {
  static char const specials[] = "NAN\0FNI\0RRE"; // packed reversed, selected by offset
  char const *const s = specials + off;
  uint_fast8_t i = 0;
  do { buf[i] = (char)(s[i] + case_adj); } while (s[++i]);
  return -(int)i;
}

// Offsets into npf_ftoa_special's packed string. Macros rather than an enum: an
// enum's underlying type is int, and narrowing that to uint_fast8_t (unsigned char
// on MSVC) trips C4244, which the Windows build treats as an error.
#define NPF_FTOA_NAN 0u
#define NPF_FTOA_INF 4u
#define NPF_FTOA_ERR 8u

#if NPF_USE_SCI == 0
static int npf_ftoa_rev(
    char *buf, npf_format_spec_t const *spec, int prec, npf_real_t f) {
  uint_fast8_t sp; sp = NPF_FTOA_ERR;
  npf_real_bin_t bin = npf_real_to_int_rep(f);

  // Unsigned -> signed int casting is IB and can raise a signal but generally doesn't.
  npf_ftoa_exp_t exp =
    (npf_ftoa_exp_t)((npf_ftoa_exp_t)(bin >> NPF_REAL_MAN_BITS) & NPF_REAL_EXP_MASK);

  bin &= ((npf_real_bin_t)0x1 << NPF_REAL_MAN_BITS) - 1;
  if (!((unsigned)(exp + 1) & NPF_REAL_EXP_MASK)) { // special value
    sp = bin ? NPF_FTOA_NAN : NPF_FTOA_INF;
    goto exit;
  }
  if (prec > (NPF_CBUF - 2)) { goto exit; }
  if (exp) { // normal number
    bin |= (npf_real_bin_t)0x1 << NPF_REAL_MAN_BITS;
  } else { // subnormal number
    ++exp;
  }
  exp = (npf_ftoa_exp_t)(exp - NPF_REAL_EXP_BIAS);

  uint_fast8_t carry; carry = 0;
  npf_ftoa_dec_t end, dec; dec = (npf_ftoa_dec_t)prec;
  if (dec
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
      || spec->alt_form
#endif
  ) {
    buf[dec++] = '.';
  }

  { // Integer part
    npf_ftoa_man_t man_i;

    if (exp >= 0) {
      int_fast8_t shift_i =
        (int_fast8_t)((exp > NPF_FTOA_SHIFT_BITS) ? (int)NPF_FTOA_SHIFT_BITS : exp);
      npf_ftoa_exp_t exp_i = (npf_ftoa_exp_t)(exp - shift_i);
      shift_i = (int_fast8_t)(NPF_REAL_MAN_BITS - shift_i);
      if (shift_i) {
        npf_real_bin_t const bin_i = NPF_BIN_SHR(bin, shift_i - 1);
        carry = (uint_fast8_t)(bin_i & 0x1);
        man_i = (npf_ftoa_man_t)(bin_i >> 1);
      } else {
        man_i = (npf_ftoa_man_t)bin;
      }

      if (exp_i) {
        exp = NPF_REAL_MAN_BITS; // invalidate the fraction part
      }

      // Scale the exponent from base-2 to base-10.
      for (; exp_i; --exp_i) {
        if (!(man_i >> (NPF_FTOA_MAN_BITS - 1))) {
          man_i = (npf_ftoa_man_t)((man_i << 1) | carry); carry = 0;
        } else {
          if (dec >= NPF_CBUF) { goto exit; }
          buf[dec++] = '0';
#if NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1
          if (sizeof(man_i) <= sizeof(uint32_t)) { // n/5 = 2*(n/10) + (n%10 >= 5)
            uint32_t const q = npf_div10((uint32_t)man_i);
            uint_fast8_t r = (uint_fast8_t)((uint32_t)man_i - (q * 10u));
            man_i = (npf_ftoa_man_t)(q * 2u);
            if (r >= 5u) { r = (uint_fast8_t)(r - 5u); ++man_i; }
            carry = (uint_fast8_t)((r + carry + 1u) >> 2);
          } else
#endif
          {
            carry = (uint_fast8_t)(((uint_fast8_t)(man_i % 5) + carry + 1u) >> 2);
            man_i /= 5;
          }
        }
      }
    } else {
      man_i = 0;
    }
    end = dec;

    // Print the integer. A 32-bit man_i has at most 10 decimal digits, so one
    // up-front capacity check replaces the per-digit check and npf_utoa_rev
    // becomes the shared decimal emitter. (Conservative: a value needing fewer
    // digits than the remaining capacity in the last 9 bytes now reports ERR.)
    if ((sizeof(npf_ftoa_man_t) <= sizeof(uint32_t)) &&
        (sizeof(npf_uint_t) >= sizeof(uint32_t))) {
      if (end > NPF_CBUF - 10) { goto exit; }
      end = (npf_ftoa_dec_t)(npf_utoa_rev_end((npf_uint_t)man_i, buf + end, 10, 0) - buf);
    } else { // man_i may be wider than npf_uint_t: emit in place
      do {
        if (end >= NPF_CBUF) { goto exit; }
        buf[end++] = (char)('0' + (char)(man_i % 10));
        man_i /= 10;
      } while (man_i);
    }
  }

  { // Fraction part
    npf_ftoa_man_t man_f;
    npf_ftoa_dec_t dec_f = (npf_ftoa_dec_t)prec;

    if (exp < NPF_REAL_MAN_BITS) {
      int_fast8_t shift_f = (int_fast8_t)((exp < 0) ? -1 : exp);
      npf_ftoa_exp_t exp_f = (npf_ftoa_exp_t)(exp - shift_f);
      npf_real_bin_t bin_f =
        NPF_BIN_SHL(bin, (NPF_REAL_BIN_BITS - NPF_REAL_MAN_BITS) + shift_f);

      // This if-else statement can be completely optimized at compile time.
      if (NPF_REAL_BIN_BITS > NPF_FTOA_MAN_BITS) {
        man_f = (npf_ftoa_man_t)(bin_f >> ((unsigned)(NPF_REAL_BIN_BITS -
                                                      NPF_FTOA_MAN_BITS) %
                                           NPF_REAL_BIN_BITS));
        carry = (uint_fast8_t)((bin_f >> ((unsigned)(NPF_REAL_BIN_BITS -
                                                     NPF_FTOA_MAN_BITS - 1) %
                                          NPF_REAL_BIN_BITS)) & 0x1);
      } else {
        man_f = (npf_ftoa_man_t)((npf_ftoa_man_t)bin_f
                                 << ((unsigned)(NPF_FTOA_MAN_BITS -
                                                NPF_REAL_BIN_BITS) % NPF_FTOA_MAN_BITS));
        carry = 0;
      }

      // Scale the exponent from base-2 to base-10 and prepare the first digit.
      for (uint_fast8_t digit = 0; dec_f && (exp_f < 4); ++exp_f) {
        if ((man_f > ((npf_ftoa_man_t)-4 / 5)) || digit) {
          carry = (uint_fast8_t)(man_f & 0x1);
          man_f = (npf_ftoa_man_t)(man_f >> 1);
        } else {
          man_f = (npf_ftoa_man_t)(man_f * 5);
          if (carry) { man_f = (npf_ftoa_man_t)(man_f + 3); carry = 0; }
          if (exp_f < 0) {
            buf[--dec_f] = '0';
          } else {
            ++digit;
          }
        }
      }
      // A precision of 0 skips the loop above, leaving man_f as raw mantissa bits that
      // can be all ones; the nudge would then wrap to 0 and drop the round-up. Every
      // loop exit path leaves headroom, so this only bites when the loop never ran.
      if (man_f != (npf_ftoa_man_t)-1) { man_f = (npf_ftoa_man_t)(man_f + carry); }
      carry = (exp_f >= 0);
      dec = 0;
    } else {
      man_f = 0;
    }

    if (dec_f) {
      // Print the fraction
      for (;;) {
        buf[--dec_f] = (char)('0' + (char)(man_f >> (NPF_FTOA_MAN_BITS - 4)));
        man_f = (npf_ftoa_man_t)(man_f & ~((npf_ftoa_man_t)0xF << (NPF_FTOA_MAN_BITS - 4)));
        if (!dec_f) { break; }
        man_f = (npf_ftoa_man_t)(man_f * 10);
      }
      man_f = (npf_ftoa_man_t)(man_f << 4);
    }
    if (exp < NPF_REAL_MAN_BITS) {
      carry &= (uint_fast8_t)(man_f >> (NPF_FTOA_MAN_BITS - 1));
      // Ties to even: an exact half rounds up only when the last kept digit is odd.
      // Reversed that is buf[0], unless '#' with precision 0 put the point there.
      if (man_f == ((npf_ftoa_man_t)0x1 << (NPF_FTOA_MAN_BITS - 1))) {
        char lsd = buf[0];
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
        if (lsd == '.') { lsd = buf[1]; }
#endif
        carry &= (uint_fast8_t)(lsd & 1);
      }
    }
  }

  // Round the number
  for (; carry; ++dec) {
    if (dec >= NPF_CBUF) { goto exit; }
    if (dec >= end) { buf[end++] = '0'; }
    char const c = buf[dec];
    if (c == '.') { continue; }
    if (c == '9') { buf[dec] = '0'; }
    else { buf[dec] = (char)(c + 1); carry = 0; }
  }

  return (int)end;
exit:
  return npf_ftoa_special(buf, spec->case_adjust, sp);
}
#endif // NPF_USE_SCI == 0

#if NPF_USE_SCI == 1

/* Scientific ('e'/'E') and shortest ('g'/'G') conversions.

   npf_ftoa_rev knows where the decimal point goes before it starts, so it can fuse
   digit generation with digit placement. These conversions can't: the decimal exponent
   isn't known until the digits have been generated *and* rounded. So the significant
   digits are generated right-aligned at the top of buf along with 'dec', the exponent
   of the least significant one, such that the value is (digits) * 10^dec. The output
   string is composed from buf[0] up afterwards.

   The base-2 to base-10 scaling is the same as npf_ftoa_rev's. The differences are
   that zeros which only carry magnitude -- the integer part's trailing zeros and the
   fraction's leading zeros -- are folded into 'dec' instead of being emitted, and that
   generation stops one digit past what the precision needs, that digit being all the
   rounding decision requires. */
static int npf_etoa_rev(char *buf, npf_format_spec_t const *spec, npf_real_t f) {
  // A 'goto exit' jumps over these, so none of them may have an initializer.
  int prec, nsig_max, nsig, dec, end, x, pe, fmode, dstop;
  npf_ftoa_exp_t exp0;
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
  int g, strip;
#endif
  uint_fast8_t sp, carry, tail;
  sp = NPF_FTOA_ERR;

  npf_real_bin_t bin = npf_real_to_int_rep(f);

  // Unsigned -> signed int casting is IB and can raise a signal but generally doesn't.
  npf_ftoa_exp_t exp =
    (npf_ftoa_exp_t)((npf_ftoa_exp_t)(bin >> NPF_REAL_MAN_BITS) & NPF_REAL_EXP_MASK);

  bin &= ((npf_real_bin_t)0x1 << NPF_REAL_MAN_BITS) - 1;
  if (!((unsigned)(exp + 1) & NPF_REAL_EXP_MASK)) { // special value
    sp = bin ? NPF_FTOA_NAN : NPF_FTOA_INF;
    goto exit;
  }

  /* 'f' bounds generation by decimal position, everything else by significant
     digit count, so each mode gets one of the two stops and disables the other. */
  prec = NPF_DEC_PREC(spec);
  /* Nothing this large fits, whichever mode runs, so bail before 'prec + 1'
     below can overflow: a wrapped nsig_max is negative, and a negative one slips
     under the significance bound rather than tripping it. */
  if (prec > (NPF_CBUF - 2)) { goto exit; }
  nsig_max = prec + 1;
  fmode = (spec->conv_spec == NPF_FMT_SPEC_CONV_FLOAT_DEC);
  dstop = INT_MIN; // unreachable, so only 'f' below arms the position stop
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
#if NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER == 1
  g = (spec->conv_spec == NPF_FMT_SPEC_CONV_FLOAT_SHORTEST);
#else
  g = !fmode; // with 'e' compiled out, whatever is not 'f' here is 'g'
#endif
  strip = 1;
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
  strip = !spec->alt_form; // '#' keeps trailing zeros and the point
#endif
  // 'g' precision counts significant digits, and 0 means 1.
  if (g) { prec = prec ? prec : 1; nsig_max = prec; }
#endif
  if (fmode) {
    /* 'f' needs every digit down to 10^-prec, so the position stop below is what
       ends generation and this bound only has to keep the fraction inside the
       buffer: nsig_max of BUF would put 'lo' at -1 and allow a write below buf. */
    nsig_max = NPF_CBUF - 1;
    dstop = -prec - 1;
  } else if (nsig_max > (NPF_CBUF - 7)) {
    // The longest output is "d.<prec digits>e+ddd". Bail before wasted work.
    goto exit;
  }

  if (exp) { // normal number
    bin |= (npf_real_bin_t)0x1 << NPF_REAL_MAN_BITS;
  } else { // subnormal number
    ++exp;
  }
  exp = (npf_ftoa_exp_t)(exp - NPF_REAL_EXP_BIAS);

  exp0 = exp;
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
regen: // only 'g' comes back here, to switch from significance to position bounds
#endif
  exp = exp0; // generation clobbers exp to invalidate the fraction part
  carry = 0;
  dec = 0;
  // An exponent the integer scaling has to walk down loses remainders, so no tie is
  // visible there. Anything else starts out able to see one; the fraction part
  // below refines this to whether the digits it generates are the whole expansion.
  tail = (uint_fast8_t)(exp0 <= NPF_FTOA_SHIFT_BITS);

  { // Integer part
    npf_ftoa_man_t man_i;

    if (exp >= 0) {
      int_fast8_t shift_i =
        (int_fast8_t)((exp > NPF_FTOA_SHIFT_BITS) ? (int)NPF_FTOA_SHIFT_BITS : exp);
      npf_ftoa_exp_t exp_i = (npf_ftoa_exp_t)(exp - shift_i);
      shift_i = (int_fast8_t)(NPF_REAL_MAN_BITS - shift_i);
      if (shift_i) {
        npf_real_bin_t const bin_i = NPF_BIN_SHR(bin, shift_i - 1);
        carry = (uint_fast8_t)(bin_i & 0x1);
        man_i = (npf_ftoa_man_t)(bin_i >> 1);
      } else {
        man_i = (npf_ftoa_man_t)bin;
      }

      if (exp_i) {
        exp = NPF_REAL_MAN_BITS; // invalidate the fraction part
      }

      // Scale the exponent from base-2 to base-10. Every '0' npf_ftoa_rev would emit
      // here is a trailing zero of the integer part, so count it in 'dec' instead.
      for (; exp_i; --exp_i) {
        if (!(man_i >> (NPF_FTOA_MAN_BITS - 1))) {
          man_i = (npf_ftoa_man_t)((man_i << 1) | carry); carry = 0;
        } else {
          ++dec;
#if NANOPRINTF_USE_DIVISION_FREE_CONVERSION == 1
          if (sizeof(man_i) <= sizeof(uint32_t)) { // n/5 = 2*(n/10) + (n%10 >= 5)
            uint32_t const q = npf_div10((uint32_t)man_i);
            uint_fast8_t r = (uint_fast8_t)((uint32_t)man_i - (q * 10u));
            man_i = (npf_ftoa_man_t)(q * 2u);
            if (r >= 5u) { r = (uint_fast8_t)(r - 5u); ++man_i; }
            carry = (uint_fast8_t)((r + carry + 1u) >> 2);
          } else
#endif
          {
            carry = (uint_fast8_t)(((uint_fast8_t)(man_i % 5) + carry + 1u) >> 2);
            man_i /= 5;
          }
        }
      }
    } else {
      man_i = 0;
    }

    // Emit the integer digits at buf[0], then right-align them at the top of buf. The
    // count isn't known until they're all out, hence the move; the destination index
    // always exceeds the source index, so the two ranges may overlap.
    end = NPF_CBUF;
    if (man_i || fmode) {
      int k;
      if ((sizeof(npf_ftoa_man_t) <= sizeof(uint32_t)) &&
          (sizeof(npf_uint_t) >= sizeof(uint32_t))) {
        k = npf_utoa_rev((npf_uint_t)man_i, buf, 10, 0); // at most 10 digits
      } else { // man_i may be wider than npf_uint_t: emit in place
        k = 0;
        do {
          if (k >= NPF_CBUF) { goto exit; }
          buf[k++] = (char)('0' + (char)(man_i % 10));
          man_i /= 10;
        } while (man_i);
      }
      for (int i = 0; i < k; ++i) {
        buf[NPF_CBUF - 1 - i] = buf[k - 1 - i];
      }
      end = NPF_CBUF - k;
    }
  }

  { // Fraction part
    // Generate one digit past the precision: with the full decimal expansion in hand,
    // rounding up is exactly "first dropped digit >= 5", so one guard digit suffices.
    int const lo = NPF_CBUF - nsig_max - 1;
    npf_ftoa_man_t man_f;

    if (exp < NPF_REAL_MAN_BITS) {
      int_fast8_t shift_f = (int_fast8_t)((exp < 0) ? -1 : exp);
      npf_ftoa_exp_t exp_f = (npf_ftoa_exp_t)(exp - shift_f);
      npf_real_bin_t bin_f =
        NPF_BIN_SHL(bin, (NPF_REAL_BIN_BITS - NPF_REAL_MAN_BITS) + shift_f);
      // A leading fraction zero is significant only if an integer digit precedes it.
      uint_fast8_t const lead = (uint_fast8_t)(end == NPF_CBUF);

      // This if-else statement can be completely optimized at compile time.
      if (NPF_REAL_BIN_BITS > NPF_FTOA_MAN_BITS) {
        man_f = (npf_ftoa_man_t)(bin_f >> ((unsigned)(NPF_REAL_BIN_BITS -
                                                      NPF_FTOA_MAN_BITS) %
                                           NPF_REAL_BIN_BITS));
        carry = (uint_fast8_t)((bin_f >> ((unsigned)(NPF_REAL_BIN_BITS -
                                                     NPF_FTOA_MAN_BITS - 1) %
                                          NPF_REAL_BIN_BITS)) & 0x1);
      } else {
        man_f = (npf_ftoa_man_t)((npf_ftoa_man_t)bin_f
                                 << ((unsigned)(NPF_FTOA_MAN_BITS -
                                                NPF_REAL_BIN_BITS) % NPF_FTOA_MAN_BITS));
        carry = 0;
      }

      /* A zero mantissa with no pending carry stays zero through everything below,
         and contributes nothing, so it can skip all of it. That is not just an
         optimization: without the guard, zero runs the loop once per binary exponent
         step, folding leading zeros into an exponent the fixup then discards, which
         leaves the most commonly printed value the slowest to print. The carry has to
         be part of the test because the scaling step adds 3 when one is pending,
         which is how a zero mantissa can still produce digits. */
      if (man_f || carry) {

      // Scale the exponent from base-2 to base-10 and prepare the first digit.
      for (uint_fast8_t digit = 0; (end > lo) && (dec > dstop) && (exp_f < 4); ++exp_f) {
        if ((man_f > ((npf_ftoa_man_t)-4 / 5)) || digit) {
          carry = (uint_fast8_t)(man_f & 0x1);
          man_f = (npf_ftoa_man_t)(man_f >> 1);
        } else {
          man_f = (npf_ftoa_man_t)(man_f * 5);
          if (carry) { man_f = (npf_ftoa_man_t)(man_f + 3); carry = 0; }
          if (exp_f < 0) {
            --dec;
            if (!lead) { buf[--end] = '0'; }
          } else {
            ++digit;
          }
        }
      }
      man_f = (npf_ftoa_man_t)(man_f + carry);
      carry = (uint_fast8_t)(exp_f >= 0);

      if ((end > lo) && (dec > dstop)) {
        // Print the fraction. Trailing zeros are implicit in 'dec', so unlike
        // npf_ftoa_rev this stops as soon as the mantissa is exhausted.
        for (;;) {
          buf[--end] = (char)('0' + (char)(man_f >> (NPF_FTOA_MAN_BITS - 4)));
          --dec;
          man_f = (npf_ftoa_man_t)(man_f & ~((npf_ftoa_man_t)0xF << (NPF_FTOA_MAN_BITS - 4)));
          if (!man_f || (end <= lo) || (dec <= dstop)) { break; }
          man_f = (npf_ftoa_man_t)(man_f * 10);
        }
        man_f = (npf_ftoa_man_t)(man_f << 4);
      }

      } // end of the nonzero-fraction path

      // If the buffer filled first, this carry is stale, but then the excess-digit
      // path below recomputes it from the first dropped digit.
      carry &= (uint_fast8_t)(man_f >> (NPF_FTOA_MAN_BITS - 1));
      // Whether the digits in buf are the whole expansion, for the tie test below.
      tail = (uint_fast8_t)!man_f;
    }
  }

  // No digits generated means the value is zero: one '0' digit at exponent 0. The
  // scaling loop above will have walked 'dec' down while chasing a nonzero digit.
  nsig = NPF_CBUF - end;
  if (!nsig) { buf[--end] = '0'; nsig = 1; dec = 0; carry = 0; }

  /* Drop the digits past what the conversion asked for and round on the first of
     them. 'f' overshoots by however far dec fell below its precision, the others
     by however many significant digits beyond the maximum they produced. */
  { int const drop = fmode ? (-prec - dec) : (nsig - nsig_max);
    if (drop > 0) {
      int i = end;
      dec += drop;
      end += drop;
      nsig -= drop;
      // A first dropped digit of '4' can never round up: the rest of the remainder
      // is under one unit in its place, so 0.4999.. + r stays below one half.
      carry = (uint_fast8_t)(buf[end - 1] >= '5');
      /* Ties to even. A tie is a dropped '5' with nothing but zeros under it, and
         it rounds up only when the last kept digit is odd. */
      if (buf[end - 1] == '5') {
        for (; tail && (i < end - 1); ++i) { tail = (uint_fast8_t)(buf[i] == '0'); }
        if (tail) { carry = (uint_fast8_t)(buf[end] & 1); }
      }
    }
  }

  for (int i = end; carry; ++i) { // Round the number
    if (i >= NPF_CBUF) {
      // Every digit was '9' and is now '0'; "999" becomes "100" with a bigger
      // exponent, so the significant digit count doesn't change.
      buf[NPF_CBUF - 1] = '1';
      ++dec;
      break;
    }
    carry = (uint_fast8_t)(buf[i] == '9');
    buf[i] = (char)(carry ? '0' : (buf[i] + 1));
  }

  x = dec + nsig - 1; // the base-10 exponent of the most significant digit

#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
  if (g) {
    // Stripping moves 'dec' and 'nsig' by the same amount, so 'x' is unaffected.
    if (strip) { while ((nsig > 1) && (buf[end] == '0')) { ++end; --nsig; ++dec; } }
    if ((x >= -4) && (x < prec)) {
      /* C11 7.21.6.1p8: style 'f' with precision prec-1-x. That wants digits down
         to a decimal position rather than a count of them, and 'f' generation also
         writes the leading zeros and the units digit that 'e' folds into the
         exponent, so this regenerates instead of relaying out the run already in
         the buffer. Same two passes npf_ftoa_rev used to provide, minus the second
         copy of the conversion. */
      int const fp = (strip ? nsig : prec) - 1 - x;
      prec = (fp > 0) ? fp : 0;
      fmode = 1;
      nsig_max = NPF_CBUF - 1;
      dstop = -prec - 1;
      g = 0; // one trip through here is enough
      goto regen;
    }
  }
#endif

  if (fmode) { /* Compose "<int>.<frac>" reversed from buf[0] up.

    Everything is derived from nsig and dec, the exponent of the least significant
    generated digit, so the digits occupy exponents dec through dec+nsig-1:
      id  generated digits that land in the integer part
      iz  integer trailing zeros, when the digits stop above the units
      fd  generated digits that land in the fraction
      fz  fraction trailing zeros, padding out to the precision
    Reversed, the order runs least significant first: fz, fd, point, iz, id. */
    int const above = nsig + ((dec > 0) ? 0 : dec); // generated digits at 10^0 or up
    int const id = (above < 0) ? 0 : above;
    int const iz = (dec > 0) ? dec : 0;             // integer zeros below the digits
    int const fd = nsig - id;                       // generated fraction digits
    int const fz = prec - fd;                       // fraction zeros past the digits
    int dp = (prec > 0);
    int o = 0, i;
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
    dp |= (spec->alt_form != 0);
#endif
    // Same lockstep argument as the 'e' layout: reads and writes both walk up, so
    // the gap is tightest at the last read and this one check covers it.
    if ((id + iz + dp + prec) > NPF_CBUF) { goto exit; }
    for (i = fz; i > 0; --i) { buf[o++] = '0'; }
    for (i = 0; i < fd; ++i) { buf[o++] = buf[end + i]; }
    buf[o] = '.'; o += dp;
    for (i = iz; i > 0; --i) { buf[o++] = '0'; }
    for (i = 0; i < id; ++i) { buf[o++] = buf[end + fd + i]; }
    return o;
  }

  { // Compose "d.<frac>e<sign><exp>" reversed from buf[0] up.
    int o;
    pe = prec; // digits after the point
#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER == 1
    if (g) { pe = (strip ? nsig : prec) - 1; }
#endif
    // The shared integer emitter already handles base 10 and the division-free path.
    o = npf_utoa_rev((npf_uint_t)((x < 0) ? -x : x), buf, 10, 0);
    if (o < 2) { buf[o++] = '0'; } // the standard mandates at least two digits
    buf[o++] = (char)((x < 0) ? '-' : '+');
    buf[o++] = (char)('E' + spec->case_adjust);

    // Reads walk up from buf[BUFSIZE - nsig] as writes walk up from buf[0]. Both
    // advance in lockstep through each run, and only writes happen between runs, so
    // the gap is smallest at the final read, where it is BUFSIZE - (output length).
    // The nsig_max check above guarantees that is not negative.
    { int const pad = pe - (nsig - 1);
      for (int i = 0; i < pe; ++i) {
        buf[o++] = (i < pad) ? '0'
          : buf[NPF_CBUF - nsig + (i - pad)];
      }
    }
    // A point is emitted when a fraction follows it, or when '#' demands it.
    { int dp = (pe > 0);
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
      dp |= (spec->alt_form != 0);
#endif
      buf[o] = '.'; o += dp; } // unconditional store, conditional advance
    buf[o++] = buf[NPF_CBUF - 1];
    return o;
  }
exit:
  return npf_ftoa_special(buf, spec->case_adjust, sp);
}

#endif // NPF_USE_SCI

#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1

// Hex float always operates on IEEE 754 binary64 (double).
// When not in single-precision mode, npf_real_* already handles double.
#if NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1
typedef uint_fast64_t npf_double_bin_t;
enum {
  NPF_DOUBLE_EXP_MASK = 2047,
  NPF_DOUBLE_EXP_BIAS = 1023,
  NPF_DOUBLE_MAN_BITS = 52,
};
static NPF_FORCE_INLINE npf_double_bin_t npf_double_to_int_rep(double f) {
  npf_double_bin_t bin = 0;
  char const *src = (char const *)&f;
  char *dst = (char *)&bin;
  for (uint_fast8_t i = 0; i < sizeof(f); ++i) { dst[i] = src[i]; }
  return bin;
}
#else
typedef npf_real_bin_t npf_double_bin_t;
#define NPF_DOUBLE_EXP_MASK NPF_REAL_EXP_MASK
#define NPF_DOUBLE_EXP_BIAS NPF_REAL_EXP_BIAS
#define NPF_DOUBLE_MAN_BITS NPF_REAL_MAN_BITS
#define npf_double_to_int_rep(f) npf_real_to_int_rep(f)
#endif

static NPF_NOINLINE int npf_atoa_rev(
    char *buf, npf_format_spec_t const *spec, double f) {
  npf_double_bin_t bin = npf_double_to_int_rep(f);
  npf_ftoa_exp_t exp =
    (npf_ftoa_exp_t)((npf_ftoa_exp_t)(bin >> NPF_DOUBLE_MAN_BITS) & NPF_DOUBLE_EXP_MASK);
  bin &= ((npf_double_bin_t)0x1 << NPF_DOUBLE_MAN_BITS) - 1;

  if (exp == (npf_ftoa_exp_t)NPF_DOUBLE_EXP_MASK) { return 0; } // caller uses ftoa_rev

  { int const n_frac_dig = (NPF_DOUBLE_MAN_BITS + 3) / 4;
    int const prec = NPF_HEX_PREC(spec);
    int end, i;

    // The longest output is "h.<prec digits>p-dddd". Past that, print "ERR".
    if (prec > (NPF_CBUF - 8)) {
      buf[0] = buf[1] = (char)('R' + spec->case_adjust); // "ERR", reversed
      buf[2] = (char)('E' + spec->case_adjust);
      return -3;
    }

    if (exp) {
      bin |= (npf_double_bin_t)0x1 << NPF_DOUBLE_MAN_BITS;
      exp = (npf_ftoa_exp_t)(exp - NPF_DOUBLE_EXP_BIAS);
    } else if (bin) {
      exp = (npf_ftoa_exp_t)(1 - NPF_DOUBLE_EXP_BIAS);
    }

    /* Discard low nibbles and round half to even, with constant shifts only. 'nib'
       ends as the last nibble discarded, its bit 0 also set if anything below it
       was nonzero. Adding the kept digit's low bit takes it past 8 exactly when
       rounding up is correct: above half, or at half with an odd digit. */
    { unsigned nib = 0; // at most 23 below, so a 16-bit int is enough
      for (i = n_frac_dig - prec; i > 0; --i) {
        nib = ((unsigned)bin & 0xFu) | ((nib + 15u) >> 4);
        bin >>= 4;
      }
      bin += (nib + ((unsigned)bin & 1u) + 7u) >> 4;
    }

    { npf_ftoa_exp_t const ae = (exp < 0) ? (npf_ftoa_exp_t)-exp : exp;
      end = npf_utoa_rev((npf_uint_t)ae, buf, 10, 0);
      buf[end++] = (exp < 0) ? '-' : '+';
      buf[end++] = (char)('P' + spec->case_adjust);
    }

    for (i = prec; i > 0; --i) { // the mantissa has 13 digits, the rest are zeros
      int_fast8_t d = 0;
      if (i <= n_frac_dig) { d = (int_fast8_t)(bin & 0xF); bin >>= 4; }
      buf[end++] = (char)(((d < 10) ? '0' : ('A' - 10 + spec->case_adjust)) + d);
    }

    if (prec > 0
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
        || spec->alt_form
#endif
    ) { buf[end++] = '.'; }

    buf[end++] = (char)('0' + (int_fast8_t)(bin & 0xF));
    return end;
  }
}

#endif // NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER

#endif // NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS

#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
static int npf_bin_len(npf_uint_t u) {
  // Return the length of the binary string format of 'u', preferring intrinsics.
  if (!u) { return 1; }

#ifdef _MSC_VER // Win64, use _BSR64 for everything. If x86, use _BSR when non-large.
  #ifdef _M_X64
    #define NPF_HAVE_BUILTIN_CLZ
    #define NPF_CLZ _BitScanReverse64
  #elif NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 0
    #define NPF_HAVE_BUILTIN_CLZ
    #define NPF_CLZ _BitScanReverse
  #endif
  #ifdef NPF_HAVE_BUILTIN_CLZ
    unsigned long idx;
    NPF_CLZ(&idx, u);
    return (int)(idx + 1);
  #endif
#elif NPF_CLANG || NPF_GCC_PAST_4_6
  #define NPF_HAVE_BUILTIN_CLZ
  #if NPF_UINT_IS_WIDE
    #define NPF_CLZ(X) ((sizeof(long long) * CHAR_BIT) - (size_t)__builtin_clzll(X))
  #else
    #define NPF_CLZ(X) ((sizeof(long) * CHAR_BIT) - (size_t)__builtin_clzl(X))
  #endif
  return (int)NPF_CLZ(u);
#endif

#ifndef NPF_HAVE_BUILTIN_CLZ
  int n;
  for (n = 0; u; ++n, u >>= 1); // slow but small software fallback
  return n;
#else
  #undef NPF_HAVE_BUILTIN_CLZ
  #undef NPF_CLZ
#endif
}
#endif

static void npf_bufputc(int c, void *ctx) {
  npf_bufputc_ctx_t *bpc = (npf_bufputc_ctx_t *)ctx;
  // NULL dst -> count-only mode (size-query semantics).
  if (bpc->dst && bpc->len) { --bpc->len; *bpc->dst++ = (char)c; }
}

#define NPF_PUTC(VAL) do { pc((int)(VAL), pc_ctx); ++npf_n; } while (0)
#define NPF_PUT(VAL) do { pc((int)(VAL), pc_ctx); } while (0)

#define NPF_EXTRACT(DST, MOD, CAST_TO, EXTRACT_AS) \
  case NPF_FMT_SPEC_LEN_MOD_##MOD: DST = (CAST_TO)va_arg(args, EXTRACT_AS); break

// When sizeof(long) == sizeof(int)
// va_arg(*args, long) and va_arg(*args, int) read the same bits, so the LONG
// case can fall into the int-promotion default.
#if LONG_MAX == INT_MAX
  #define NPF_LONG_IS_INT 1
#else
  #define NPF_LONG_IS_INT 0
#endif

// 'z' and 't' are rarely distinct widths from 'int' or 'long'; fold them into
// whichever same-width case already exists instead of paying for their own
// va_arg sites.
#if SIZE_MAX == UINT_MAX
  #define NPF_LM_Z_INT   case NPF_FMT_SPEC_LEN_MOD_LARGE_SIZET:
  #define NPF_LM_Z_LONG
  #define NPF_LM_Z_OWN 0
#elif SIZE_MAX == ULONG_MAX && !NPF_LONG_IS_INT
  #define NPF_LM_Z_INT
  #define NPF_LM_Z_LONG  case NPF_FMT_SPEC_LEN_MOD_LARGE_SIZET:
  #define NPF_LM_Z_OWN 0
#else
  #define NPF_LM_Z_INT
  #define NPF_LM_Z_LONG
  #define NPF_LM_Z_OWN 1
#endif

#if PTRDIFF_MAX == INT_MAX
  #define NPF_LM_T_INT   case NPF_FMT_SPEC_LEN_MOD_LARGE_PTRDIFFT:
  #define NPF_LM_T_LONG
  #define NPF_LM_T_OWN 0
#elif PTRDIFF_MAX == LONG_MAX && !NPF_LONG_IS_INT
  #define NPF_LM_T_INT
  #define NPF_LM_T_LONG  case NPF_FMT_SPEC_LEN_MOD_LARGE_PTRDIFFT:
  #define NPF_LM_T_OWN 0
#else
  #define NPF_LM_T_INT
  #define NPF_LM_T_LONG
  #define NPF_LM_T_OWN 1
#endif

int npf_vpprintf(npf_putc pc, void *pc_ctx, char const *format, va_list args) {
  npf_format_spec_t fs;
  char const *cur = format;
  int npf_n = 0;

  while (*cur) {
    char const *const fs_end =
      (*cur != '%') ? 0 : npf_parse_format_spec_end(cur, &fs);
    if (!fs_end) { NPF_PUTC(*cur++); continue; }
    cur = fs_end;

    // Extract star-args immediately
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
    /* The one ceiling both a literal and a star width pass through, so the digit
       loop does not need its own. The magnitude is taken in unsigned because
       INT_MIN has no positive int counterpart; that is also the only magnitude the
       signed compare below could not hold, so it is the only one pinned here. */
    if (fs.field_width_opt == NPF_FMT_SPEC_OPT_STAR) {
      unsigned w = (unsigned)va_arg(args, int);
      if ((int)w < 0) { w = 0u - w; fs.left_justified = 1; }
      fs.field_width = (int)((w > (unsigned)INT_MAX) ? (unsigned)NPF_FMT_NUM_MAX : w);
    }
    if (fs.field_width > NPF_FMT_NUM_MAX) { fs.field_width = NPF_FMT_NUM_MAX; }
#endif
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
    if (fs.prec_opt == NPF_FMT_SPEC_OPT_STAR) {
      fs.prec = va_arg(args, int);
      if (fs.prec < 0) { fs.prec_opt = NPF_FMT_SPEC_OPT_NONE; }
    }
    if (fs.prec > NPF_FMT_NUM_MAX) { fs.prec = NPF_FMT_NUM_MAX; }
#endif

#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
  // Set default precision (we can do that only now that we have extracted the
  // argument-provided precision (".*"), and know whether to ignore that or not.
  if (fs.prec_opt == NPF_FMT_SPEC_OPT_NONE) {
    fs.prec = 0; // a discarded precision ("%.-3d", negative ".*") must not leak
    if (fs.conv_spec == NPF_FMT_SPEC_CONV_POINTER) {
      fs.prec = (sizeof(void *) * CHAR_BIT + 3) / 4;
    }
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
    // float specs are last in the enum; a single range check identifies them.
    else if (fs.conv_spec >= NPF_FMT_SPEC_CONV_FLOAT_DEC) {
#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1
      fs.prec = (fs.conv_spec == NPF_FMT_SPEC_CONV_FLOAT_HEX)
        ? (NPF_DOUBLE_MAN_BITS + 3) / 4 : 6;
#else
      fs.prec = 6;
#endif
    }
#endif
  }
#endif

#if (NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1) && \
    (NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1)
    // For b B d i o u x X, the '0' flag must be ignored if a precision is provided.
    // Those conversions are contiguous in the enum.
    if ((fs.prec_opt != NPF_FMT_SPEC_OPT_NONE) &&
        (fs.conv_spec >= NPF_FMT_SPEC_CONV_SIGNED_INT) &&
        (fs.conv_spec <= NPF_FMT_SPEC_CONV_UNSIGNED_INT)) { fs.leading_zero_pad = 0; }
#endif

    union { char cbuf_mem[NPF_CBUF]; npf_uint_t binval; } u;
    char *cbuf = u.cbuf_mem, sign_c = 0;
    int cbuf_len = 0;
    char need_0x = 0;
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
    int field_pad = 0;
    char pad_c = 0;
#endif
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
    int prec_pad = 0;
#endif

    // Extract and convert the argument to string, point cbuf at the text.
    // Range checks for INT and FLOAT families avoid an 11-entry dispatch table.
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
    if (fs.conv_spec >= NPF_FMT_SPEC_CONV_FLOAT_DEC) {
      npf_real_t val;
#if NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1
      val = va_arg(args, npf_float_t).val;
#elif LDBL_MANT_DIG == DBL_MANT_DIG
      // long double has the same representation as double
      // no need to branch on the 'L' length modifier.
      val = va_arg(args, double);
#else
      if (fs.length_modifier == NPF_FMT_SPEC_LEN_MOD_LONG_DOUBLE) {
        val = (npf_real_t)va_arg(args, long double);
      } else {
        val = va_arg(args, double);
      }
#endif

      sign_c = (npf_real_to_int_rep(val) >> NPF_REAL_SIGN_POS) ? '-' : fs.prepend;
#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER == 1
      if ((fs.conv_spec == NPF_FMT_SPEC_CONV_FLOAT_HEX) &&
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
          ((cbuf_len = npf_atoa_rev(cbuf, &fs, (double)val)) != 0)) {
        if (cbuf_len > 0) { need_0x = (char)('X' + fs.case_adjust); } // not "ERR"
#else
          ((cbuf_len = npf_atoa_rev(cbuf, &fs, (double)val)) > 0)) {
        need_0x = (char)('X' + fs.case_adjust);
#endif
      } else
#endif
#if NPF_USE_SCI == 1
      { cbuf_len = npf_etoa_rev(cbuf, &fs, val); }
#else
      { cbuf_len = npf_ftoa_rev(cbuf, &fs, NPF_DEC_PREC(&fs), val); }
#endif
      if (cbuf_len < 0) { // negative means text (not number), so ignore the '0' flag
         cbuf_len = -cbuf_len;
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
         fs.leading_zero_pad = 0;
#endif
      }
    } else
#endif
#if NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS == 1
    if (fs.conv_spec == NPF_FMT_SPEC_CONV_WRITEBACK) {
      void *wb = va_arg(args, void *);
      switch (fs.length_modifier) {
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
        NPF_LM_Z_INT NPF_LM_T_INT
#endif
        case NPF_FMT_SPEC_LEN_MOD_NONE: *(int *)wb = npf_n; break;
#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 1
        case NPF_FMT_SPEC_LEN_MOD_SHORT: *(short *)wb = (short)npf_n; break;
        case NPF_FMT_SPEC_LEN_MOD_CHAR: *(signed char *)wb = (signed char)npf_n; break;
#endif
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
        NPF_LM_Z_LONG NPF_LM_T_LONG
#endif
        case NPF_FMT_SPEC_LEN_MOD_LONG: *(long *)wb = (long)npf_n; break;
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
        case NPF_FMT_SPEC_LEN_MOD_LARGE_LONG_LONG: *(long long *)wb = (long long)npf_n; break;
        case NPF_FMT_SPEC_LEN_MOD_LARGE_INTMAX: *(intmax_t *)wb = (intmax_t)npf_n; break;
  #if NPF_LM_Z_OWN
        case NPF_FMT_SPEC_LEN_MOD_LARGE_SIZET: *(npf_ssize_t *)wb = (npf_ssize_t)npf_n; break;
  #endif
  #if NPF_LM_T_OWN
        case NPF_FMT_SPEC_LEN_MOD_LARGE_PTRDIFFT: *(ptrdiff_t *)wb = (ptrdiff_t)npf_n; break;
  #endif
#endif
        default: break;
      }
    } else
#endif
    if (fs.conv_spec >= NPF_FMT_SPEC_CONV_SIGNED_INT) {
      npf_uint_t val;
      uint_fast8_t base = 10u;

      if (fs.conv_spec == NPF_FMT_SPEC_CONV_SIGNED_INT) {
        npf_int_t sval = 0;
#if !NPF_LONG_IS_INT || NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
        switch (fs.length_modifier) {
#if !NPF_LONG_IS_INT
  #if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
          NPF_LM_Z_LONG NPF_LM_T_LONG
  #endif
          NPF_EXTRACT(sval, LONG, long, long);
#endif
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
          NPF_EXTRACT(sval, LARGE_LONG_LONG, long long, long long);
          NPF_EXTRACT(sval, LARGE_INTMAX, intmax_t, intmax_t);
  #if NPF_LM_Z_OWN
          NPF_EXTRACT(sval, LARGE_SIZET, npf_ssize_t, npf_ssize_t);
  #endif
  #if NPF_LM_T_OWN
          NPF_EXTRACT(sval, LARGE_PTRDIFFT, ptrdiff_t, ptrdiff_t);
  #endif
          NPF_LM_Z_INT NPF_LM_T_INT
#endif
          default:
#endif
          {
            int v = va_arg(args, int);
#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 1
            if (fs.length_modifier == NPF_FMT_SPEC_LEN_MOD_SHORT) { v = (short)v; }
            else if (fs.length_modifier == NPF_FMT_SPEC_LEN_MOD_CHAR) { v = (signed char)v; }
#endif
            sval = v;
          }
#if !NPF_LONG_IS_INT || NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
          break;
        }
#endif
        sign_c = (sval < 0) ? '-' : fs.prepend;
        val = (npf_uint_t)sval;
        if (sval < 0) { val = 0 - val; }
      } else {
        if (fs.conv_spec == NPF_FMT_SPEC_CONV_POINTER) {
          val = (npf_uint_t)(uintptr_t)va_arg(args, void *);
          base = 16u;
        } else {
#if !NPF_LONG_IS_INT || NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
          switch (fs.length_modifier) {
#if !NPF_LONG_IS_INT
  #if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
            NPF_LM_Z_LONG NPF_LM_T_LONG
  #endif
            NPF_EXTRACT(val, LONG, unsigned long, unsigned long);
#endif
#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
            NPF_EXTRACT(val, LARGE_LONG_LONG, unsigned long long, unsigned long long);
            NPF_EXTRACT(val, LARGE_INTMAX, uintmax_t, uintmax_t);
  #if NPF_LM_Z_OWN
            NPF_EXTRACT(val, LARGE_SIZET, size_t, size_t);
  #endif
  #if NPF_LM_T_OWN
            NPF_EXTRACT(val, LARGE_PTRDIFFT, npf_uptrdiff_t, npf_uptrdiff_t);
  #endif
            NPF_LM_Z_INT NPF_LM_T_INT
#endif
            default:
#endif
            {
              unsigned v = va_arg(args, unsigned);
#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS == 1
              if (fs.length_modifier == NPF_FMT_SPEC_LEN_MOD_SHORT) { v = (unsigned short)v; }
              else if (fs.length_modifier == NPF_FMT_SPEC_LEN_MOD_CHAR) { v = (unsigned char)v; }
#endif
              val = v;
            }
#if !NPF_LONG_IS_INT || NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS == 1
            break;
          }
#endif
          if (fs.conv_spec == NPF_FMT_SPEC_CONV_OCTAL) { base = 8u; }
          else if (fs.conv_spec == NPF_FMT_SPEC_CONV_HEX_INT) { base = 16u; }
        }
      }

#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
      if (!val && (fs.prec_opt != NPF_FMT_SPEC_OPT_NONE) && !fs.prec) {
        // cbuf_len was initialized to 0; preserved here.
#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
        if (base == 8u && fs.alt_form) { fs.prec = 1; } // octal '#' special
#endif
      } else
#endif
      {
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
        if (fs.conv_spec == NPF_FMT_SPEC_CONV_BINARY) {
          cbuf_len = npf_bin_len(val); u.binval = val;
        } else
#endif
        { cbuf_len = npf_utoa_rev(val, cbuf, base, fs.case_adjust); }

#if NANOPRINTF_USE_ALT_FORM_FLAG == 1
        if (val && fs.alt_form) {
          if (base == 8u) {
            cbuf[cbuf_len++] = '0';
          } else if (base == 16u) {
            need_0x = (char)('X' + fs.case_adjust);
          }
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
          else if (fs.conv_spec == NPF_FMT_SPEC_CONV_BINARY) {
            need_0x = (char)('B' + fs.case_adjust);
          }
#endif
        }
#endif
      }
    } else if (fs.conv_spec == NPF_FMT_SPEC_CONV_STRING) {
      cbuf = va_arg(args, char *);
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
      for (char const *s = cbuf;
           ((fs.prec_opt == NPF_FMT_SPEC_OPT_NONE) || (cbuf_len < fs.prec)) && cbuf && *s;
           ++s, ++cbuf_len);
#else
      for (char const *s = cbuf; cbuf && *s; ++s, ++cbuf_len); // strlen
#endif
    } else {
      // PERCENT or CHAR: produce a 1-char buffer.
      *cbuf = (fs.conv_spec == NPF_FMT_SPEC_CONV_CHAR) ? (char)va_arg(args, int) : '%';
      cbuf_len = 1;
    }

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
    // Compute the field width pad character: '0' flag only with numeric types, and
    // '-' overrides '0'. An integer with a precision already dropped the '0' flag.
    // With no field width, field_pad clamps to 0 below, so pad_c is never used.
    pad_c = ' ';
    if (fs.leading_zero_pad && !fs.left_justified) { pad_c = '0'; }

#endif

    // Compute the number of bytes to truncate or '0'-pad. Skip for STRING
    // (already handled by precision-limited length) and FLOAT (precision is
    // after the decimal point; float specs are last in the enum).
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
    if ((fs.conv_spec != NPF_FMT_SPEC_CONV_STRING)
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS == 1
        && (fs.conv_spec < NPF_FMT_SPEC_CONV_FLOAT_DEC)
#endif
       ) { prec_pad = NPF_MAX(0, fs.prec - cbuf_len); }
#endif

    // Total bytes this conversion emits; npf_n is bulk-updated at the end.
    int spec_len = cbuf_len + !!sign_c + (need_0x ? 2 : 0)
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
                   + prec_pad
#endif
                   ;

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
    // Given the full converted length, how many pad bytes?
    field_pad = fs.field_width - spec_len;
    if (field_pad < 0) { field_pad = 0; }
    spec_len += field_pad;

    // Right-justified padding: zero-pad goes AFTER sign/0x; space-pad goes BEFORE.
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
    // '0'-padding is contiguous with the leading precision zeros, so fold it into
    // prec_pad; sign/0x is then emitted exactly once below. prec_pad stays 0 for
    // STRING unless folded into, so the hoisted loop is a no-op there.
    if (pad_c == '0') {
      prec_pad += field_pad;
      field_pad = 0;
    }
#else
    if (pad_c == '0') {
      if (sign_c) { NPF_PUT(sign_c); sign_c = 0; }
      if (need_0x) { NPF_PUT('0'); NPF_PUT(need_0x); need_0x = 0; }
    }
#endif
    if (!fs.left_justified) {
      while (field_pad-- > 0) { NPF_PUT(pad_c); }
    }
#endif
    if (sign_c) { NPF_PUT(sign_c); }
    if (need_0x) { NPF_PUT('0'); NPF_PUT(need_0x); }
#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS == 1
    while (prec_pad-- > 0) { NPF_PUT('0'); } // leading zeros: precision and '0' pad
#endif

    // Write the converted payload. The STRING parse loop guarantees cbuf_len == 0
    // when cbuf is NULL, so the output loop can elide the `cbuf &&` check.
    if (fs.conv_spec == NPF_FMT_SPEC_CONV_STRING) {
      for (int i = 0; i < cbuf_len; ++i) { NPF_PUT(cbuf[i]); }
    } else {
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS == 1
      if (fs.conv_spec == NPF_FMT_SPEC_CONV_BINARY) {
        while (cbuf_len) { NPF_PUT('0' + ((u.binval >> --cbuf_len) & 1)); }
      } else
#endif
      { while (cbuf_len-- > 0) { NPF_PUT(cbuf[cbuf_len]); } } // payload is reversed
    }

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS == 1
    // Apply left-justified field width. The right-justified loop above has
    // already run field_pad below zero in the non-left-justified case, so
    // this loop body only executes for left-justified specifiers.
    while (field_pad-- > 0) { NPF_PUT(pad_c); }
#endif
    // NPF_PUT emissions don't tally npf_n; add the conversion's total length in bulk.
    npf_n += spec_len;
  }

  return npf_n;
}

#undef NPF_PUTC
#undef NPF_PUT
#undef NPF_EXTRACT
#undef NPF_LONG_IS_INT
#undef NPF_BIN_SHR
#undef NPF_BIN_SHL
#undef NPF_FTOA_NAN
#undef NPF_FTOA_INF
#undef NPF_FTOA_ERR
#ifdef NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST
  #undef NPF_FMT_SPEC_CONV_FLOAT_SCI_FIRST
#endif
#undef NPF_USE_SCI

int npf_vsnprintf(char * NPF_RESTRICT buffer,
                  size_t bufsz,
                  char const * NPF_RESTRICT format,
                  va_list vlist) {
  npf_bufputc_ctx_t bufputc_ctx = { buffer, bufsz };
  int const n = npf_vpprintf(npf_bufputc, &bufputc_ctx, format, vlist);

  if (buffer && bufsz) {
    // npf_vpprintf never returns negative (no encoding errors possible).
#ifdef NANOPRINTF_SNPRINTF_SAFE_EMPTY_STRING_ON_OVERFLOW
    buffer[(unsigned)n >= bufsz ? 0 : (unsigned)n] = '\0';
#else
    buffer[NPF_MIN((unsigned)n, bufsz - 1)] = '\0';
#endif
  }

  return n;
}

int npf_pprintf_(npf_putc pc,
                     void * NPF_RESTRICT pc_ctx,
                     char const * NPF_RESTRICT format,
                     ...) {
  va_list val;
  va_start(val, format);
  int const rv = npf_vpprintf(pc, pc_ctx, format, val);
  va_end(val);
  return rv;
}

int npf_snprintf_(char * NPF_RESTRICT buffer,
                      size_t bufsz,
                      const char * NPF_RESTRICT format,
                      ...) {
  va_list val;
  va_start(val, format);
  int const rv = npf_vsnprintf(buffer, bufsz, format, val);
  va_end(val);
  return rv;
}

#if NPF_HAVE_GCC_WARNING_PRAGMAS
  #pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
  #pragma warning(pop)
#endif

#endif // NPF_IMPLEMENTATION_INCLUDED
#endif // NANOPRINTF_IMPLEMENTATION

// Single-precision argument wrapping and MAP macro expansion machinery.
// The npf_snprintf / npf_pprintf / NPF_MAP_ARGS macros defined above reference
// these, but that's fine: macro bodies are only expanded at the point of
// invocation, not at the point of definition.

#ifndef NPF_MAP_INCLUDED
#define NPF_MAP_INCLUDED

#if defined(NANOPRINTF_USE_FLOAT_SINGLE_PRECISION) && \
    (NANOPRINTF_USE_FLOAT_SINGLE_PRECISION == 1)

// NPF__WRAP: wrap float/double args into npf_float_t, pass other types through.
// C++ uses function overloading; C uses _Generic with function-pointer selection.
// _Generic picks a function, then (x) calls it. Non-selected branches are bare
// function names (always valid), sidestepping _Generic's type-check-all-branches
// behavior that would reject (float)(string_ptr) in non-selected branches.
#if defined(__cplusplus)
  extern "C++" {
    static inline npf_float_t npf__wrap_impl(float f) {
      npf_float_t r; r.val = f; return r;
    }
    static inline npf_float_t npf__wrap_impl(double d) {
      npf_float_t r; r.val = (float)d; return r;
    }
    template<typename T> static inline T npf__wrap_impl(T v) { return v; }
  }
  #define NPF__WRAP(x) npf__wrap_impl(x)
#elif defined(__GNUC__) || defined(__clang__)
  #define NPF__IS_REAL(x) \
    (__builtin_types_compatible_p(__typeof__(0 ? (x) : (x)), float) || \
     __builtin_types_compatible_p(__typeof__(0 ? (x) : (x)), double))
  #define NPF__WRAP(x) __builtin_choose_expr(NPF__IS_REAL(x), \
    ({ npf_float_t _npf_r; \
       _npf_r.val = (float)__builtin_choose_expr(NPF__IS_REAL(x), (x), 0); \
       _npf_r; }), \
    (x))
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  static inline npf_float_t npf__wf(float f) {
    npf_float_t r; r.val = f; return r;
  }
  static inline npf_float_t npf__wd(double d) {
    npf_float_t r; r.val = (float)d; return r;
  }
  static inline npf_float_t npf__wld(long double d) {
    npf_float_t r; r.val = (float)d; return r;
  }

  static inline int npf__id_i(int v) { return v; }
  static inline unsigned npf__id_u(unsigned v) { return v; }
  static inline long npf__id_l(long v) { return v; }
  static inline unsigned long npf__id_ul(unsigned long v) { return v; }
  static inline long long npf__id_ll(long long v) { return v; }
  static inline unsigned long long npf__id_ull(unsigned long long v) { return v; }
  static inline char npf__id_c(char v) { return v; }
  static inline signed char npf__id_sc(signed char v) { return v; }
  static inline unsigned char npf__id_uc(unsigned char v) { return v; }
  static inline short npf__id_s(short v) { return v; }
  static inline unsigned short npf__id_us(unsigned short v) { return v; }
  static inline char *npf__id_cp(char *v) { return v; }
  static inline char const *npf__id_ccp(char const *v) { return v; }
  static inline void *npf__id_vp(void *v) { return v; }
  static inline void const *npf__id_cvp(void const *v) { return v; }
  static inline int *npf__id_ip(int *v) { return v; }
  static inline short *npf__id_sp(short *v) { return v; }
  static inline long *npf__id_lp(long *v) { return v; }
  static inline long long *npf__id_llp(long long *v) { return v; }
  static inline signed char *npf__id_scp(signed char *v) { return v; }

  #define NPF__WRAP(x) _Generic((x), \
    float:              npf__wf, \
    double:             npf__wd, \
    long double:        npf__wld, \
    int:                npf__id_i, \
    unsigned:           npf__id_u, \
    long:               npf__id_l, \
    unsigned long:      npf__id_ul, \
    long long:          npf__id_ll, \
    unsigned long long: npf__id_ull, \
    char:               npf__id_c, \
    signed char:        npf__id_sc, \
    unsigned char:      npf__id_uc, \
    short:              npf__id_s, \
    unsigned short:     npf__id_us, \
    char *:             npf__id_cp, \
    char const *:       npf__id_ccp, \
    void *:             npf__id_vp, \
    void const *:       npf__id_cvp, \
    int *:              npf__id_ip, \
    short *:            npf__id_sp, \
    long *:             npf__id_lp, \
    long long *:        npf__id_llp, \
    signed char *:      npf__id_scp)(x)
#else
  #error Single-precision float wrapping requires C11, GCC/Clang, or C++.
#endif

// Argument counting (up to 64 variadic args)
#define NPF__NARG(...)  NPF__NARG_(__VA_ARGS__, NPF__RSEQ())
#define NPF__NARG_(...) NPF__ARG_N(__VA_ARGS__)
#define NPF__ARG_N( \
   _1, _2, _3, _4, _5, _6, _7, _8, _9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
  _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
  _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
  _61,_62,_63,_64,N,...) N
#define NPF__RSEQ() \
  64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38, \
  37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11, \
  10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

// Token pasting
#define NPF__CAT(a,b)  NPF__CAT_(a,b)
#define NPF__CAT_(a,b) a##b

// MAP: apply f to each argument
#define NPF__MAP(f,...) NPF__CAT(NPF__MAP_,NPF__NARG(__VA_ARGS__))(f,__VA_ARGS__)
#define NPF__MAP_1(f,a)      f(a)
#define NPF__MAP_2(f,a,...)  f(a), NPF__MAP_1(f,__VA_ARGS__)
#define NPF__MAP_3(f,a,...)  f(a), NPF__MAP_2(f,__VA_ARGS__)
#define NPF__MAP_4(f,a,...)  f(a), NPF__MAP_3(f,__VA_ARGS__)
#define NPF__MAP_5(f,a,...)  f(a), NPF__MAP_4(f,__VA_ARGS__)
#define NPF__MAP_6(f,a,...)  f(a), NPF__MAP_5(f,__VA_ARGS__)
#define NPF__MAP_7(f,a,...)  f(a), NPF__MAP_6(f,__VA_ARGS__)
#define NPF__MAP_8(f,a,...)  f(a), NPF__MAP_7(f,__VA_ARGS__)
#define NPF__MAP_9(f,a,...)  f(a), NPF__MAP_8(f,__VA_ARGS__)
#define NPF__MAP_10(f,a,...) f(a), NPF__MAP_9(f,__VA_ARGS__)
#define NPF__MAP_11(f,a,...) f(a), NPF__MAP_10(f,__VA_ARGS__)
#define NPF__MAP_12(f,a,...) f(a), NPF__MAP_11(f,__VA_ARGS__)
#define NPF__MAP_13(f,a,...) f(a), NPF__MAP_12(f,__VA_ARGS__)
#define NPF__MAP_14(f,a,...) f(a), NPF__MAP_13(f,__VA_ARGS__)
#define NPF__MAP_15(f,a,...) f(a), NPF__MAP_14(f,__VA_ARGS__)
#define NPF__MAP_16(f,a,...) f(a), NPF__MAP_15(f,__VA_ARGS__)
#define NPF__MAP_17(f,a,...) f(a), NPF__MAP_16(f,__VA_ARGS__)
#define NPF__MAP_18(f,a,...) f(a), NPF__MAP_17(f,__VA_ARGS__)
#define NPF__MAP_19(f,a,...) f(a), NPF__MAP_18(f,__VA_ARGS__)
#define NPF__MAP_20(f,a,...) f(a), NPF__MAP_19(f,__VA_ARGS__)
#define NPF__MAP_21(f,a,...) f(a), NPF__MAP_20(f,__VA_ARGS__)
#define NPF__MAP_22(f,a,...) f(a), NPF__MAP_21(f,__VA_ARGS__)
#define NPF__MAP_23(f,a,...) f(a), NPF__MAP_22(f,__VA_ARGS__)
#define NPF__MAP_24(f,a,...) f(a), NPF__MAP_23(f,__VA_ARGS__)
#define NPF__MAP_25(f,a,...) f(a), NPF__MAP_24(f,__VA_ARGS__)
#define NPF__MAP_26(f,a,...) f(a), NPF__MAP_25(f,__VA_ARGS__)
#define NPF__MAP_27(f,a,...) f(a), NPF__MAP_26(f,__VA_ARGS__)
#define NPF__MAP_28(f,a,...) f(a), NPF__MAP_27(f,__VA_ARGS__)
#define NPF__MAP_29(f,a,...) f(a), NPF__MAP_28(f,__VA_ARGS__)
#define NPF__MAP_30(f,a,...) f(a), NPF__MAP_29(f,__VA_ARGS__)
#define NPF__MAP_31(f,a,...) f(a), NPF__MAP_30(f,__VA_ARGS__)
#define NPF__MAP_32(f,a,...) f(a), NPF__MAP_31(f,__VA_ARGS__)
#define NPF__MAP_33(f,a,...) f(a), NPF__MAP_32(f,__VA_ARGS__)
#define NPF__MAP_34(f,a,...) f(a), NPF__MAP_33(f,__VA_ARGS__)
#define NPF__MAP_35(f,a,...) f(a), NPF__MAP_34(f,__VA_ARGS__)
#define NPF__MAP_36(f,a,...) f(a), NPF__MAP_35(f,__VA_ARGS__)
#define NPF__MAP_37(f,a,...) f(a), NPF__MAP_36(f,__VA_ARGS__)
#define NPF__MAP_38(f,a,...) f(a), NPF__MAP_37(f,__VA_ARGS__)
#define NPF__MAP_39(f,a,...) f(a), NPF__MAP_38(f,__VA_ARGS__)
#define NPF__MAP_40(f,a,...) f(a), NPF__MAP_39(f,__VA_ARGS__)
#define NPF__MAP_41(f,a,...) f(a), NPF__MAP_40(f,__VA_ARGS__)
#define NPF__MAP_42(f,a,...) f(a), NPF__MAP_41(f,__VA_ARGS__)
#define NPF__MAP_43(f,a,...) f(a), NPF__MAP_42(f,__VA_ARGS__)
#define NPF__MAP_44(f,a,...) f(a), NPF__MAP_43(f,__VA_ARGS__)
#define NPF__MAP_45(f,a,...) f(a), NPF__MAP_44(f,__VA_ARGS__)
#define NPF__MAP_46(f,a,...) f(a), NPF__MAP_45(f,__VA_ARGS__)
#define NPF__MAP_47(f,a,...) f(a), NPF__MAP_46(f,__VA_ARGS__)
#define NPF__MAP_48(f,a,...) f(a), NPF__MAP_47(f,__VA_ARGS__)
#define NPF__MAP_49(f,a,...) f(a), NPF__MAP_48(f,__VA_ARGS__)
#define NPF__MAP_50(f,a,...) f(a), NPF__MAP_49(f,__VA_ARGS__)
#define NPF__MAP_51(f,a,...) f(a), NPF__MAP_50(f,__VA_ARGS__)
#define NPF__MAP_52(f,a,...) f(a), NPF__MAP_51(f,__VA_ARGS__)
#define NPF__MAP_53(f,a,...) f(a), NPF__MAP_52(f,__VA_ARGS__)
#define NPF__MAP_54(f,a,...) f(a), NPF__MAP_53(f,__VA_ARGS__)
#define NPF__MAP_55(f,a,...) f(a), NPF__MAP_54(f,__VA_ARGS__)
#define NPF__MAP_56(f,a,...) f(a), NPF__MAP_55(f,__VA_ARGS__)
#define NPF__MAP_57(f,a,...) f(a), NPF__MAP_56(f,__VA_ARGS__)
#define NPF__MAP_58(f,a,...) f(a), NPF__MAP_57(f,__VA_ARGS__)
#define NPF__MAP_59(f,a,...) f(a), NPF__MAP_58(f,__VA_ARGS__)
#define NPF__MAP_60(f,a,...) f(a), NPF__MAP_59(f,__VA_ARGS__)
#define NPF__MAP_61(f,a,...) f(a), NPF__MAP_60(f,__VA_ARGS__)
#define NPF__MAP_62(f,a,...) f(a), NPF__MAP_61(f,__VA_ARGS__)
#define NPF__MAP_63(f,a,...) f(a), NPF__MAP_62(f,__VA_ARGS__)
#define NPF__MAP_64(f,a,...) f(a), NPF__MAP_63(f,__VA_ARGS__)

#endif // NANOPRINTF_USE_FLOAT_SINGLE_PRECISION

#endif // NPF_MAP_INCLUDED

/*
  nanoprintf is dual-licensed under both the "Unlicense" and the
  "Zero-Clause BSD" (0BSD) licenses. The intent of this dual-licensing
  structure is to make nanoprintf as consumable as possible in as many
  environments / countries / companies as possible without any
  encumberances.

  The text of the two licenses follows below:

  ============================== UNLICENSE ==============================

  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org>

  ================================ 0BSD =================================

  Copyright (C) 2019- by Charles Nicholson <charles.nicholson+nanoprintf@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for
  any purpose with or without fee is hereby granted.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// }}}

#endif
