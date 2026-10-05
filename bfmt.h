// vim: set foldmethod=marker foldlevel=0:
#ifndef BFMT_H
#define BFMT_H

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef BFMT_API
#define BFMT_API
#endif

#define bfmt_print(OUT, ...) \
	bfmt__print(OUT, (bfmt__element_t[]){ bfmt__map(bfmt__to_element, __VA_ARGS__) {} })

#define bfmt_println(OUT, ...) bfmt_print(OUT, __VA_ARGS__ __VA_OPT__(,) "\n")

#define bfmt_translate(OUT, LOCALE, ...) \
	bfmt__translate(OUT, LOCALE, bfmt__map(bfmt__translate_item, __VA_ARGS__))

#define bfmt_register_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE) \
	typedef VALUE_TYPE bfmt__formatter_value_type(FORMATTER_NAME); \
	typedef OPTIONS_TYPE bfmt__formatter_options_type(FORMATTER_NAME); \
	void bfmt__formatter_wrapper(FORMATTER_NAME)(bfmt_ctx_t* ctx, const void* value, const void* options) { \
		FORMATTER_NAME(ctx, *(bfmt__formatter_value_type(FORMATTER_NAME)*)value, *(bfmt__formatter_options_type(FORMATTER_NAME)*)options); \
   	}

#define bfmt_decl_formatter(VALUE_TYPE, FORMATTER_NAME, OPTIONS_TYPE) \
	extern bfmt_formatter(value_type, formatter_name, OPTIONS_TYPE); \
	bfmt_register_formatter(value_type, formatter_name, OPTIONS_TYPE)

#define bfmt_formatter(VALUE_TYPE, NAME, OPTIONS_TYPE) \
	void NAME(bfmt_ctx_t* ctx, VALUE_TYPE value, OPTIONS_TYPE options)

#define bfmt_with(VALUE, FORMATTER, ...) \
	( \
		(void)sizeof((__typeof__(FORMATTER((bfmt_ctx_t*)0, VALUE, bfmt__options(bfmt__formatter_options_type(FORMATTER), __VA_ARGS__)[0]))*)0), \
		(bfmt__element_t){ \
			.value = (bfmt__formatter_value_type(FORMATTER)[1]){ VALUE }, \
			.formatter = bfmt__formatter_wrapper(FORMATTER), \
			.options = bfmt__options(bfmt__formatter_options_type(FORMATTER), __VA_ARGS__), \
		} \
	)

#define bfmt(VALUE, ...) \
	((bfmt__element_t){ \
		.value = (bfmt__typeof(VALUE)[1]){ VALUE }, \
		.formatter = bfmt__formatter_for(VALUE), \
		.options = bfmt__options(bfmt_options_t, __VA_ARGS__), \
	})

typedef struct bfmt_ctx_s bfmt_ctx_t;

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
	int width;
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
	bfmt_alignment_t align;
	bfmt_sign_style_t sign;
	bool show_base;
	bool zero_pad;
	bool uppercase;
	int width;
	bool with_precision;
	int precision;

	union {
		bfmt_float_style_t float_style;
		bfmt_base_t int_base;
	};
} bfmt_options_t;

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

// Internal {{{

// Macro helpers {{{

#define bfmt__map(F, ...) __VA_OPT__(bfmt__expand(bfmt__map_helper(F, __VA_ARGS__)))
#define bfmt__map_helper(F, ARG, ...) F(ARG) __VA_OPT__(bfmt__map_again bfmt__parens (F, __VA_ARGS__))
#define bfmt__map_again() bfmt__map_helper
#define bfmt__parens ()
#define bfmt__expand(...)  bfmt__expand3(bfmt__expand3(__VA_ARGS__))
#define bfmt__expand3(...) bfmt__expand2(bfmt__expand2(__VA_ARGS__))
#define bfmt__expand2(...) bfmt__expand1(bfmt__expand1(__VA_ARGS__))
#define bfmt__expand1(...) __VA_ARGS__

#define bfmt__concat(LHS, RHS) bfmt__concat2(LHS, RHS)
#define bfmt__concat2(LHS, RHS) LHS##RHS

#define bfmt__typeof(VALUE) __typeof__(0 ? (VALUE) : (VALUE))

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

#define bfmt__types(X) \
	X(const char*, str) \
	X(char*, str) \
	X(char, char) \
	X(short, short) \
	X(unsigned short, ushort) \
	X(int, int) \
	X(unsigned int, uint) \
	X(long, long) \
	X(unsigned long, ulong) \
	X(long long, longlong) \
	X(unsigned long long, ulonglong) \
	X(float, float) \
	X(double, double) \
	X(long double, longdouble) \
	X(const void*, pointer) \
	X(void*, pointer) \
	X(bool, bool) \
	X(bfmt__element_t, element)

#define bfmt__formatter_for(VALUE) _Generic(VALUE bfmt__types(bfmt__type_to_formatter))
#define bfmt__type_to_formatter(TYPE, SUFFIX) , TYPE: &bfmt__concat(bfmt__format_, SUFFIX)

#define bfmt__declare_formatter(TYPE, SUFFIX) extern bfmt__formatter_t bfmt__concat(bfmt__format_, SUFFIX);
bfmt__types(bfmt__declare_formatter)

#define bfmt__formatter_value_type(formatter_name) bfmt__##formatter_name##_type_t
#define bfmt__formatter_options_type(formatter_name) bfmt__##formatter_name##_options_t
#define bfmt__formatter_wrapper(formatter_name) bfmt__##formatter_name##_wrapper

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
#include <alloca.h>

// Format {{{

#define bfmt_printf(CTX, OPTIONS, ARG) \
	do { \
		if (OPTIONS.width > 0 && OPTIONS.with_precision) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.width, OPTIONS.precision, ARG); \
		} else if (OPTIONS.width > 0) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.width, ARG); \
		} else if (OPTIONS.with_precision) { \
			(bfmt_fmt)(ctx, fmt, OPTIONS.precision, ARG); \
		} else { \
			(bfmt_fmt)(ctx, fmt, ARG); \
		} \
	} while (0)

static void
bfmt_build_fmt(char* cursor, const char* length, char specifier, bfmt_options_t options) {
	*cursor++ = '%';

	if (options.align == BFMT_ALIGN_LEFT) {
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

	if (options.width > 0) {
		*cursor++ = '*';
	}

	if (options.with_precision) {
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
bfmt__int_specifier(bfmt_options_t options, char decimal) {
	switch (options.int_base) {
		case BFMT_BASE_DEC: return decimal;
		case BFMT_BASE_OCT: return 'o';
		case BFMT_BASE_HEX: return options.uppercase ? 'X' : 'x';
	}
}

static char
bfmt__float_specifier(bfmt_options_t options) {
	switch(options.float_style) {
		case BFMT_FLOAT_FIXED: return options.uppercase ? 'F' : 'f';
		case BFMT_FLOAT_EXPONENT: return options.uppercase ? 'E' : 'e';
		case BFMT_FLOAT_HEX: return options.uppercase ? 'A' : 'a';
		case BFMT_FLOAT_GENERAL: return options.uppercase ? 'G' : 'g';
	}
}

void
bfmt__format_str(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) {
	const char* value = *(const char**)value_ref;
	bfmt_options_t options = *(bfmt_options_t*)options_ref;

	if (options.with_precision || options.width > 0) {  // Fancy formatting
		char fmt[16];
		bfmt_build_fmt(fmt, "", 's', options);
		bfmt_printf(ctx, options, value);
	} else {  // Direct write
		bfmt_write(ctx, value, (int)strlen(value));
	}
}

void
bfmt__format_char(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) {
	char value = *(char*)value_ref;
	bfmt_options_t options = *(bfmt_options_t*)options_ref;

	char fmt[16];
	bfmt_build_fmt(fmt, "", 'c', options);
	bfmt_printf(ctx, options, value);
}

void
bfmt__format_pointer(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) {
	const void* value = *(const void**)value_ref;
	bfmt_options_t options = *(bfmt_options_t*)options_ref;

	char fmt[16];
	bfmt_build_fmt(fmt, "", 'p', options);
	bfmt_printf(ctx, options, value);
}

void
bfmt__format_bool(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) {
	bool value = *(bool*)value_ref;
	bfmt_options_t options = *(bfmt_options_t*)options_ref;

	char fmt[16];
	bfmt_build_fmt(fmt, "", 's', options);
	bfmt_printf(ctx, options, value ? "true" : "false");
}

#define bfmt__format_int_type(TYPE, SUFFIX, LENGTH, SPECIFIER) \
	void bfmt__concat(bfmt__format_, SUFFIX)(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) { \
		TYPE value = *(TYPE*)value_ref; \
		bfmt_options_t options = *(bfmt_options_t*)options_ref; \
		char fmt[16]; \
		bfmt_build_fmt(fmt, LENGTH, bfmt__int_specifier(options, SPECIFIER), options); \
		bfmt_printf(ctx, options, value); \
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
	void bfmt__concat(bfmt__format_, SUFFIX)(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) { \
		TYPE value = *(TYPE*)value_ref; \
		bfmt_options_t options = *(bfmt_options_t*)options_ref; \
		char fmt[16]; \
		bfmt_build_fmt(fmt, LENGTH, bfmt__float_specifier(options), options); \
		bfmt_printf(ctx, options, value); \
	}

bfmt__format_float_type(float, float, "")
bfmt__format_float_type(double, double, "")
bfmt__format_float_type(long double, longdouble, "L")

void
bfmt__format_element(bfmt_ctx_t* ctx, const void* value_ref, const void* options_ref) {
	bfmt__element_t element = *(bfmt__element_t*)value_ref;
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

void
bfmt_fmtv(bfmt_ctx_t* ctx, const char* fmt, va_list args) {
	int buffer_capacity = (int)sizeof(ctx->buf);
	int buffer_free = buffer_capacity - ctx->bytes_buffered;

	va_list args_copy;
	va_copy(args_copy, args);

	int format_size = vsnprintf(&ctx->buf[ctx->bytes_buffered], buffer_free, fmt, args);
	if (format_size >= 0) {
		if (format_size < buffer_free) {  // New bytes fit in the buffer (without null terminator)
			ctx->bytes_buffered += format_size;
		} else { // Not enough space
			ctx->stream->write(ctx->stream->userdata, ctx->buf, ctx->bytes_buffered);
			ctx->bytes_buffered = 0;

			if (buffer_capacity > format_size) {
				ctx->bytes_buffered += vsnprintf(ctx->buf, buffer_capacity, fmt, args_copy);
			} else {
				char* fmt_buf = alloca(format_size + 1);
				vsnprintf(fmt_buf, format_size + 1, fmt, args_copy);
				ctx->stream->write(ctx->stream->userdata, fmt_buf, format_size);
			}
		}
	}

	va_end(args_copy);
}

static void
bfmt_write_file(void* userdata, const char* str, int len) {
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
		.write = bfmt_write_file,
	};
}

static void
bfmt_write_stdout(void* userdata, const char* str, int len) {
	bfmt_write_file(stdout, str, len);
}

static void
bfmt_write_stderr(void* userdata, const char* str, int len) {
	bfmt_write_file(stderr, str, len);
}

bfmt_stream_t* bfmt_stdout = &(bfmt_stream_t){
	.write = bfmt_write_stdout,
};

bfmt_stream_t* bfmt_stderr = &(bfmt_stream_t){
	.write = bfmt_write_stderr,
};

// }}}

#endif
