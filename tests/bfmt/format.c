#include <stdbool.h>

// A type that bfmt picks up on its own once it is listed in BFMT_USER_TYPES
typedef struct {
	int x;
	int y;
} bfmt_test_point_t;

typedef struct {
	bool brackets;
} bfmt_test_point_options_t;

#define BFMT_USER_TYPES(X) \
	X(bfmt_test_point_t, bfmt_test_format_point, bfmt_test_point_options_t)

#include "shared.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static btest_suite_t format = {
	.name = "bfmt/format",
	.init_per_test = init_output,
};

// Custom formatters {{{

bfmt_decl_formatter(bfmt_test_point_t, bfmt_test_format_point, bfmt_test_point_options_t)

// Prints through the context with bfmt_print
bfmt_formatter(bfmt_test_point_t, bfmt_test_format_point, bfmt_test_point_options_t) {
	if (options.brackets) {
		bfmt_print(ctx, "[", value.x, ", ", value.y, "]");
	} else {
		bfmt_print(ctx, "(", value.x, ", ", value.y, ")");
	}
}

// An alternative formatter for a builtin type, only reachable with bfmt_with
typedef struct {
	int decimals;
} bfmt_test_percent_options_t;

bfmt_decl_formatter(double, bfmt_test_format_percent, bfmt_test_percent_options_t)

// Prints through the context with bfmt_fmt
bfmt_formatter(double, bfmt_test_format_percent, bfmt_test_percent_options_t) {
	bfmt_fmt(ctx, "%.*f%%", options.decimals, value * 100.0);
}

// Writes `value` copies of "0123456789" with bfmt_write
bfmt_decl_formatter(int, bfmt_test_format_digits, bfmt_no_options_t)

bfmt_formatter(int, bfmt_test_format_digits, bfmt_no_options_t) {
	(void)options;
	for (int i = 0; i < value; ++i) {
		bfmt_write(ctx, "0123456789", 10);
	}
}

// Writes the numbers below `value` with bfmt_fmt
bfmt_decl_formatter(int, bfmt_test_format_count_up, bfmt_no_options_t)

bfmt_formatter(int, bfmt_test_format_count_up, bfmt_no_options_t) {
	(void)options;
	for (int i = 0; i < value; ++i) {
		bfmt_fmt(ctx, "%d,", i);
	}
}

// }}}

// Basic values {{{

BTEST(format, nothing) {
	bfmt_print(OUT);
	EXPECT_OUTPUT("");
	BTEST_EXPECT_EQUAL("%d", fixture.num_writes, 0);

	bfmt_println(OUT);
	EXPECT_OUTPUT("\n");
}

BTEST(format, strings) {
	const char* constant = "constant";
	char buf[] = "mutable";
	char* mutable = buf;

	bfmt_print(OUT, "literal ", constant, " ", mutable);
	EXPECT_OUTPUT("literal constant mutable");

	bfmt_print(OUT, "");
	EXPECT_OUTPUT("");
}

BTEST(format, println) {
	bfmt_println(OUT, "a", "b");
	bfmt_println(OUT, 1);
	EXPECT_OUTPUT("ab\n1\n");
}

BTEST(format, integers) {
	short s = -1;
	unsigned short us = 2;
	int i = -3;
	unsigned int ui = 4;
	long l = -5;
	unsigned long ul = 6;
	long long ll = -7;
	unsigned long long ull = 8;

	bfmt_print(OUT, s, " ", us, " ", i, " ", ui, " ", l, " ", ul, " ", ll, " ", ull);
	EXPECT_OUTPUT("-1 2 -3 4 -5 6 -7 8");
}

BTEST(format, integer_limits) {
	bfmt_print(OUT, INT_MIN, " ", INT_MAX, " ", UINT_MAX);
	EXPECT_OUTPUT("-2147483648 2147483647 4294967295");

	bfmt_print(OUT, LLONG_MIN, " ", LLONG_MAX, " ", ULLONG_MAX);
	EXPECT_OUTPUT("-9223372036854775808 9223372036854775807 18446744073709551615");

	short smin = SHRT_MIN;
	unsigned short usmax = USHRT_MAX;
	bfmt_print(OUT, smin, " ", usmax);
	EXPECT_OUTPUT("-32768 65535");
}

BTEST(format, floats) {
	float f = 1.5f;
	double d = -2.25;
	long double ld = 3.125L;

	bfmt_print(OUT, f, " ", d, " ", ld);
	EXPECT_OUTPUT("1.500000 -2.250000 3.125000");
}

BTEST(format, chars_and_bools) {
	char c = 'x';
	bool yes = true;
	bool no = false;

	bfmt_print(OUT, c, " ", yes, " ", no);
	EXPECT_OUTPUT("x true false");
}

BTEST(format, pointers) {
	int target = 0;
	void* ptr = &target;
	const void* const_ptr = &target;

	// Zero-padded hex digits with no prefix, unlike most libc
	char expected[64];
	int num_digits = (int)(2 * sizeof(void*));
	unsigned long long address = (unsigned long long)(uintptr_t)ptr;
	snprintf(expected, sizeof(expected), "%0*llx %0*llx", num_digits, address, num_digits, address);

	bfmt_print(OUT, ptr, " ", const_ptr);
	EXPECT_OUTPUT(expected);

	void* null = NULL;
	snprintf(expected, sizeof(expected), "%0*d", num_digits, 0);
	bfmt_print(OUT, null);
	EXPECT_OUTPUT(expected);
}

BTEST(format, many_arguments) {
	// 17 elements is the most a single call can take
	bfmt_print(OUT, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
	EXPECT_OUTPUT("1234567891011121314151617");

	bfmt_println(OUT, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	EXPECT_OUTPUT("12345678910111213141516\n");
}

BTEST(format, arguments_are_evaluated_once) {
	int counter = 0;
	bfmt_print(OUT, "<", counter++, ">");
	BTEST_EXPECT_EQUAL("%d", counter, 1);
	EXPECT_OUTPUT("<0>");

	bfmt_print(OUT, bfmt(counter++, { .layout.min_width = 2 }));
	BTEST_EXPECT_EQUAL("%d", counter, 2);
	EXPECT_OUTPUT(" 1");
}

// }}}

// Options {{{

BTEST(format, int_base) {
	bfmt_print(OUT, bfmt(255, { .base = BFMT_BASE_HEX }));
	EXPECT_OUTPUT("ff");

	bfmt_print(OUT, bfmt(255, { .base = BFMT_BASE_HEX, .uppercase = true }));
	EXPECT_OUTPUT("FF");

	bfmt_print(OUT, bfmt(8, { .base = BFMT_BASE_OCT }));
	EXPECT_OUTPUT("10");

	bfmt_print(OUT, bfmt(255, { .base = BFMT_BASE_DEC }));
	EXPECT_OUTPUT("255");

	bfmt_print(OUT, bfmt(255u, { .base = BFMT_BASE_HEX }));
	EXPECT_OUTPUT("ff");

	bfmt_print(OUT, bfmt(-1, { .base = BFMT_BASE_HEX }));
	EXPECT_OUTPUT("ffffffff");

	bfmt_print(OUT, bfmt(0xdeadbeefcafeull, { .base = BFMT_BASE_HEX }));
	EXPECT_OUTPUT("deadbeefcafe");
}

BTEST(format, int_show_base) {
	bfmt_print(OUT, bfmt(255, { .base = BFMT_BASE_HEX, .show_base = true }));
	EXPECT_OUTPUT("0xff");

	bfmt_print(OUT, bfmt(255, { .base = BFMT_BASE_HEX, .show_base = true, .uppercase = true }));
	EXPECT_OUTPUT("0XFF");

	bfmt_print(OUT, bfmt(8, { .base = BFMT_BASE_OCT, .show_base = true }));
	EXPECT_OUTPUT("010");
}

BTEST(format, int_sign) {
	bfmt_print(OUT, bfmt(1, { .sign = BFMT_SIGN_IF_NEEDED }), " ", bfmt(-1, { .sign = BFMT_SIGN_IF_NEEDED }));
	EXPECT_OUTPUT("1 -1");

	bfmt_print(OUT, bfmt(1, { .sign = BFMT_SIGN_ALWAYS }), " ", bfmt(-1, { .sign = BFMT_SIGN_ALWAYS }));
	EXPECT_OUTPUT("+1 -1");

	bfmt_print(OUT, bfmt(1, { .sign = BFMT_SIGN_SPACE_IF_PLUS }), "|", bfmt(-1, { .sign = BFMT_SIGN_SPACE_IF_PLUS }));
	EXPECT_OUTPUT(" 1|-1");
}

BTEST(format, int_layout) {
	bfmt_print(OUT, "[", bfmt(42, { .layout.min_width = 6 }), "]");
	EXPECT_OUTPUT("[    42]");

	bfmt_print(OUT, "[", bfmt(42, { .layout = { .min_width = 6, .align = BFMT_ALIGN_LEFT } }), "]");
	EXPECT_OUTPUT("[42    ]");

	bfmt_print(OUT, "[", bfmt(-42, { .layout.min_width = 6, .zero_pad = true }), "]");
	EXPECT_OUTPUT("[-00042]");

	// Never truncates
	bfmt_print(OUT, "[", bfmt(123456, { .layout.min_width = 3 }), "]");
	EXPECT_OUTPUT("[123456]");
}

BTEST(format, int_precision) {
	bfmt_print(OUT, bfmt(42, { .precision = bfmt_precision(5) }));
	EXPECT_OUTPUT("00042");

	bfmt_print(OUT, "[", bfmt(42, { .precision = bfmt_precision(4), .layout.min_width = 7 }), "]");
	EXPECT_OUTPUT("[   0042]");
}

BTEST(format, float_precision) {
	bfmt_print(OUT, bfmt(3.14159, { .precision = bfmt_precision(2) }));
	EXPECT_OUTPUT("3.14");

	bfmt_print(OUT, bfmt(2.5, { .precision = bfmt_precision(0) }));
	EXPECT_OUTPUT("2");

	bfmt_print(OUT, bfmt(1.0f, { .precision = bfmt_precision(1) }));
	EXPECT_OUTPUT("1.0");
}

BTEST(format, float_style) {
	bfmt_print(OUT, bfmt(1234.5, { .style = BFMT_FLOAT_FIXED }));
	EXPECT_OUTPUT("1234.500000");

	bfmt_print(OUT, bfmt(1234.5, { .style = BFMT_FLOAT_EXPONENT, .precision = bfmt_precision(3) }));
	EXPECT_OUTPUT("1.234e+03");

	bfmt_print(OUT, bfmt(1234.5, { .style = BFMT_FLOAT_EXPONENT, .precision = bfmt_precision(3), .uppercase = true }));
	EXPECT_OUTPUT("1.234E+03");

	bfmt_print(OUT, bfmt(1234.5, { .style = BFMT_FLOAT_GENERAL }));
	EXPECT_OUTPUT("1234.5");

	// Without a precision, every digit of the mantissa is printed
	bfmt_print(OUT, bfmt(1.0, { .style = BFMT_FLOAT_HEX }));
	EXPECT_OUTPUT("0x1.0000000000000p+0");

	bfmt_print(OUT, bfmt(1.5, { .style = BFMT_FLOAT_HEX, .precision = bfmt_precision(1) }));
	EXPECT_OUTPUT("0x1.8p+0");

	bfmt_print(OUT, bfmt(1.5, { .style = BFMT_FLOAT_HEX, .precision = bfmt_precision(1), .uppercase = true }));
	EXPECT_OUTPUT("0X1.8P+0");
}

BTEST(format, float_layout_and_sign) {
	bfmt_print(OUT, "[", bfmt(2.5, { .layout.min_width = 8, .precision = bfmt_precision(2) }), "]");
	EXPECT_OUTPUT("[    2.50]");

	bfmt_print(OUT, "[", bfmt(2.5, { .layout = { .min_width = 8, .align = BFMT_ALIGN_LEFT }, .precision = bfmt_precision(2) }), "]");
	EXPECT_OUTPUT("[2.50    ]");

	bfmt_print(OUT, "[", bfmt(-2.5, { .layout.min_width = 8, .precision = bfmt_precision(2), .zero_pad = true }), "]");
	EXPECT_OUTPUT("[-0002.50]");

	bfmt_print(OUT, bfmt(2.5, { .sign = BFMT_SIGN_ALWAYS, .precision = bfmt_precision(1) }));
	EXPECT_OUTPUT("+2.5");
}

BTEST(format, float_special_values) {
	double zero = 0.0;
	double inf = 1.0 / zero;
	double nan = zero / zero;

	bfmt_print(OUT, inf, " ", -inf);
	EXPECT_OUTPUT("inf -inf");

	bfmt_print(OUT, bfmt(inf, { .uppercase = true }));
	EXPECT_OUTPUT("INF");

	bfmt_print(OUT, nan);
	BTEST_EXPECT_EX(strstr(fixture.buf, "nan") != NULL, "got \"%s\"", fixture.buf);
	reset_output();
}

BTEST(format, str_options) {
	bfmt_print(OUT, "[", bfmt("abc", { .layout.min_width = 6 }), "]");
	EXPECT_OUTPUT("[   abc]");

	bfmt_print(OUT, "[", bfmt("abc", { .layout = { .min_width = 6, .align = BFMT_ALIGN_LEFT } }), "]");
	EXPECT_OUTPUT("[abc   ]");

	bfmt_print(OUT, "[", bfmt("abcdef", { .precision = bfmt_precision(2) }), "]");
	EXPECT_OUTPUT("[ab]");

	bfmt_print(OUT, "[", bfmt("abcdef", { .precision = bfmt_precision(2), .layout.min_width = 4 }), "]");
	EXPECT_OUTPUT("[  ab]");

	bfmt_print(OUT, "[", bfmt("abcdef", { .precision = bfmt_precision(0) }), "]");
	EXPECT_OUTPUT("[]");
}

BTEST(format, char_and_bool_options) {
	char c = 'x';
	bfmt_print(OUT, "[", bfmt(c, { .layout.min_width = 3 }), "]");
	EXPECT_OUTPUT("[  x]");

	bfmt_print(OUT, "[", bfmt(c, { .layout = { .min_width = 3, .align = BFMT_ALIGN_LEFT } }), "]");
	EXPECT_OUTPUT("[x  ]");

	bool yes = true;
	bfmt_print(OUT, "[", bfmt(yes, { .layout.min_width = 6 }), "]");
	EXPECT_OUTPUT("[  true]");

	bfmt_print(OUT, "[", bfmt(yes, { .precision = bfmt_precision(1) }), "]");
	EXPECT_OUTPUT("[t]");
}

BTEST(format, options_as_a_value) {
	bfmt_int_options_t hex = { .base = BFMT_BASE_HEX, .show_base = true };
	bfmt_print(OUT, bfmt(255, hex), " ", bfmt(16, hex));
	EXPECT_OUTPUT("0xff 0x10");

	// No options is the same as plain printing
	bfmt_print(OUT, bfmt(255), " ", 255);
	EXPECT_OUTPUT("255 255");
}

// }}}

// Elements and custom formatters {{{

BTEST(format, elements_are_values) {
	// The same thing bfmt_print builds for each of its arguments
	bfmt__element_t padded = bfmt(7, { .layout.min_width = 3, .zero_pad = true });
	bfmt_print(OUT, padded, "-", padded);
	EXPECT_OUTPUT("007-007");
}

BTEST(format, user_type) {
	bfmt_test_point_t point = { .x = 1, .y = -2 };

	bfmt_print(OUT, "at ", point);
	EXPECT_OUTPUT("at (1, -2)");

	bfmt_print(OUT, "at ", bfmt(point, { .brackets = true }));
	EXPECT_OUTPUT("at [1, -2]");
}

BTEST(format, with_formatter) {
	bfmt_print(OUT, bfmt_with(bfmt_test_format_percent, 0.5));
	EXPECT_OUTPUT("50%");

	bfmt_print(OUT, bfmt_with(bfmt_test_format_percent, 0.125, { .decimals = 1 }), " done");
	EXPECT_OUTPUT("12.5% done");

	bfmt_test_point_t point = { .x = 3, .y = 4 };
	bfmt_print(OUT, bfmt_with(bfmt_test_format_point, point, { .brackets = true }));
	EXPECT_OUTPUT("[3, 4]");
}

// }}}

// Streams {{{

BTEST(format, output_is_buffered) {
	bfmt_print(OUT, "a", 1, "b", 2, "c", 3);
	BTEST_EXPECT_EQUAL("%d", fixture.num_writes, 1);
	EXPECT_OUTPUT("a1b2c3");
}

BTEST(format, long_string) {
	// Larger than the internal buffer
	char big[601];
	for (int i = 0; i < 600; ++i) { big[i] = (char)('a' + i % 26); }
	big[600] = '\0';

	char expected[700];
	snprintf(expected, sizeof(expected), "<%s>", big);

	const char* str = big;
	bfmt_print(OUT, "<", str, ">");
	EXPECT_OUTPUT(expected);
}

BTEST(format, long_padded_string) {
	char expected[402];
	memset(expected, ' ', 400);
	expected[399] = 'x';
	expected[400] = '|';
	expected[401] = '\0';

	bfmt_print(OUT, bfmt("x", { .layout.min_width = 400 }), "|");
	EXPECT_OUTPUT(expected);
}

BTEST(format, many_small_writes) {
	char expected[1024] = "";
	for (int i = 0; i < 100; ++i) { strcat(expected, "0123456789"); }

	bfmt_print(OUT, bfmt_with(bfmt_test_format_digits, 100));
	EXPECT_OUTPUT(expected);
}

BTEST(format, many_formatted_writes) {
	char expected[2048] = "";
	int len = 0;
	for (int i = 0; i < 300; ++i) {
		len += snprintf(expected + len, sizeof(expected) - (size_t)len, "%d,", i);
	}

	bfmt_print(OUT, "<", bfmt_with(bfmt_test_format_count_up, 300), ">");
	BTEST_EXPECT(fixture.buf[0] == '<');
	BTEST_EXPECT(fixture.len == len + 2 && fixture.buf[fixture.len - 1] == '>');
	fixture.buf[fixture.len - 1] = '\0';
	BTEST_EXPECT_EX(strcmp(fixture.buf + 1, expected) == 0, "got \"%s\"", fixture.buf + 1);
	reset_output();
}

BTEST(format, file_stream) {
	FILE* file = tmpfile();
	BTEST_ASSERT(file != NULL);

	bfmt_stream_t stream = bfmt_wrap_file(file);
	bfmt_println(&stream, "to a file: ", 42);
	bfmt_print(&stream, "second line");

	char content[64] = { 0 };
	rewind(file);
	size_t num_read = fread(content, 1, sizeof(content) - 1, file);
	fclose(file);

	BTEST_EXPECT_EQUAL("%zu", num_read, strlen("to a file: 42\nsecond line"));
	BTEST_EXPECT_EX(strcmp(content, "to a file: 42\nsecond line") == 0, "got \"%s\"", content);
}

// }}}
