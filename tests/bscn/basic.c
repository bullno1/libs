#include "../../bscn.h"
#include "../../btest.h"

static btest_suite_t basic = {
	.name = "bscn/basic",
};

BTEST(basic, accept_char) {
	bscn_t scn = bscn_make_cstr("a=b");

	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_char(&scn, 'a').len, 1);

	// A failed match returns an empty view at the cursor and does not move it
	bscn_str_t miss = bscn_accept_char(&scn, 'x');
	BTEST_EXPECT(bscn_str_is_empty(miss));
	BTEST_EXPECT(miss.chars == scn.input.chars + 1);
	BTEST_EXPECT_EQUAL("%d", (int)bscn_pos(&scn).offset, 1);

	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_char(&scn, '=').len, 1);
	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_char(&scn, 'b').len, 1);
	BTEST_EXPECT(bscn_is_done(&scn));

	// Accept at the end of input
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_char(&scn, 'b')));
}

BTEST(basic, accept_str) {
	bscn_t scn = bscn_make_cstr("http://example.com");

	BTEST_EXPECT(bscn_str_eq(bscn_accept_str(&scn, BSCN_STR("http")), BSCN_STR("http")));

	// All or nothing
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_str(&scn, BSCN_STR(":/x"))));
	BTEST_EXPECT_EQUAL("%d", (int)bscn_pos(&scn).offset, 4);

	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept_str(&scn, BSCN_STR("://"))));
	BTEST_EXPECT(bscn_str_eq(bscn_remaining(&scn), BSCN_STR("example.com")));
}

BTEST(basic, accept_while) {
	bscn_t scn = bscn_make_cstr("  hello_1 world");

	bscn_str_t spaces = bscn_accept_while(&scn, bscn_space);
	BTEST_EXPECT_EQUAL("%d", (int)spaces.len, 2);

	bscn_str_t ident = bscn_accept_while(&scn, bscn_identifier);
	BTEST_EXPECT(bscn_str_eq(ident, BSCN_STR("hello_1")));

	// Matching nothing is an empty view, cursor stays put
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_while(&scn, bscn_identifier)));
	BTEST_EXPECT_EQUAL("%d", (int)bscn_pos(&scn).offset, 9);
}

BTEST(basic, accept_until) {
	bscn_t scn = bscn_make_cstr("key]=value");

	// The delimiter itself is not consumed
	bscn_str_t key = bscn_accept_until_char(&scn, ']');
	BTEST_EXPECT(bscn_str_eq(key, BSCN_STR("key")));
	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_char(&scn, ']').len, 1);

	// A missing delimiter consumes the rest of the input
	bscn_str_t rest = bscn_accept_until_char(&scn, ']');
	BTEST_EXPECT(bscn_str_eq(rest, BSCN_STR("=value")));
	BTEST_EXPECT(bscn_is_done(&scn));
}

BTEST(basic, accept_until_str) {
	bscn_t scn = bscn_make_cstr("http://example.com");

	bscn_str_t scheme = bscn_accept_until_str(&scn, BSCN_STR("://"));
	BTEST_EXPECT(bscn_str_eq(scheme, BSCN_STR("http")));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept_str(&scn, BSCN_STR("://"))));

	bscn_str_t rest = bscn_accept_until_str(&scn, BSCN_STR("://"));
	BTEST_EXPECT(bscn_str_eq(rest, BSCN_STR("example.com")));
	BTEST_EXPECT(bscn_is_done(&scn));
}

BTEST(basic, accept_until_pred) {
	bscn_t scn = bscn_make_cstr("value; comment");

	bscn_str_t value = bscn_accept_until_pred(&scn, bscn_space);
	BTEST_EXPECT(bscn_str_eq(value, BSCN_STR("value;")));
}

BTEST(basic, peek) {
	bscn_t scn = bscn_make_cstr("abc");

	// Peek never moves the cursor
	BTEST_EXPECT_EQUAL("%d", (int)bscn_peek_char(&scn, 'a').len, 1);
	BTEST_EXPECT(bscn_str_is_empty(bscn_peek_char(&scn, 'b')));
	BTEST_EXPECT_EQUAL("%d", (int)bscn_peek_str(&scn, BSCN_STR("ab")).len, 2);
	BTEST_EXPECT(bscn_str_is_empty(bscn_peek_str(&scn, BSCN_STR("abcd"))));
	BTEST_EXPECT_EQUAL("%d", (int)bscn_peek_while(&scn, bscn_identifier).len, 3);
	BTEST_EXPECT_EQUAL("%d", (int)bscn_pos(&scn).offset, 0);
}

BTEST(basic, generic_macros) {
	bscn_t scn = bscn_make_cstr("[section] ; note");

	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept(&scn, '[')));
	BTEST_EXPECT(bscn_str_eq(bscn_accept(&scn, bscn_identifier), BSCN_STR("section")));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept(&scn, "]")));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept(&scn, BSCN_STR(" "))));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_peek(&scn, ';')));
	BTEST_EXPECT(bscn_str_eq(bscn_accept_until(&scn, "note"), BSCN_STR("; ")));
	BTEST_EXPECT(bscn_str_eq(bscn_accept_until(&scn, bscn_space), BSCN_STR("note")));
}

BTEST(basic, str_helpers) {
	BTEST_EXPECT(bscn_str_is_empty(BSCN_STR("")));
	BTEST_EXPECT(bscn_str_eq(BSCN_STR(""), BSCN_STR("")));
	BTEST_EXPECT(!bscn_str_eq(BSCN_STR("ab"), BSCN_STR("abc")));
	BTEST_EXPECT(!bscn_str_eq(BSCN_STR("ab"), BSCN_STR("ac")));

	BTEST_EXPECT(bscn_str_eq(
		bscn_str_trim(BSCN_STR("  hi \t "), bscn_space),
		BSCN_STR("hi")
	));
	BTEST_EXPECT(bscn_str_is_empty(bscn_str_trim(BSCN_STR("   "), bscn_space)));
}

BTEST(basic, empty_input) {
	bscn_t scn = bscn_make(BSCN_STR(""));

	BTEST_EXPECT(bscn_is_done(&scn));
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_while(&scn, bscn_space)));
	BTEST_EXPECT(bscn_str_is_empty(bscn_next_line(&scn)));
	BTEST_EXPECT(bscn_str_is_empty(bscn_remaining(&scn)));
}
