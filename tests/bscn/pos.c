#include "../../bscn.h"
#include "../../btest.h"

static btest_suite_t pos = {
	.name = "bscn/pos",
};

static void
expect_pos(bscn_pos_t actual, bscn_index_t offset, bscn_index_t line, bscn_index_t column) {
	BTEST_EXPECT_EQUAL("%d", (int)actual.offset, (int)offset);
	BTEST_EXPECT_EQUAL("%d", (int)actual.line, (int)line);
	BTEST_EXPECT_EQUAL("%d", (int)actual.column, (int)column);
}

BTEST(pos, tracking) {
	bscn_t scn = bscn_make_cstr("ab\ncd");

	expect_pos(bscn_pos(&scn), 0, 1, 1);

	bscn_accept_until_char(&scn, '\n');
	expect_pos(bscn_pos(&scn), 2, 1, 3);

	bscn_accept_new_line(&scn);
	expect_pos(bscn_pos(&scn), 3, 2, 1);

	bscn_accept_str(&scn, BSCN_STR("cd"));
	expect_pos(bscn_pos(&scn), 5, 2, 3);
}

BTEST(pos, tracking_bulk) {
	// A single accept spanning multiple lines still tracks correctly
	bscn_t scn = bscn_make_cstr("one\ntwo\nthree");

	bscn_accept_while(&scn, bscn_non_space);
	bscn_accept_while(&scn, bscn_space);
	bscn_accept_until_str(&scn, BSCN_STR("ree"));
	expect_pos(bscn_pos(&scn), 10, 3, 3);
}

BTEST(pos, next_line) {
	bscn_t scn = bscn_make_cstr("one\n\nthree");

	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("one")));
	expect_pos(bscn_pos(&scn), 4, 2, 1);

	// A blank line is a valid, empty result
	BTEST_EXPECT(bscn_str_is_empty(bscn_next_line(&scn)));
	BTEST_EXPECT(!bscn_is_done(&scn));

	// The last line does not need a terminator
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("three")));
	BTEST_EXPECT(bscn_is_done(&scn));
}

BTEST(pos, crlf) {
	bscn_t scn = bscn_make_cstr("one\r\ntwo\nthree\r\n");

	// The `\r` is stripped from the view along with the terminator
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("one")));
	expect_pos(bscn_pos(&scn), 5, 2, 1);

	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("two")));
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("three")));
	BTEST_EXPECT(bscn_is_done(&scn));
	expect_pos(bscn_pos(&scn), 16, 4, 1);
}

BTEST(pos, accept_new_line) {
	bscn_t scn = bscn_make_cstr("\r\n\nx\r");

	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_new_line(&scn).len, 2);
	expect_pos(bscn_pos(&scn), 2, 2, 1);

	BTEST_EXPECT_EQUAL("%d", (int)bscn_accept_new_line(&scn).len, 1);
	expect_pos(bscn_pos(&scn), 3, 3, 1);

	// Not at a terminator
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_new_line(&scn)));
	bscn_accept_char(&scn, 'x');

	// A lone `\r` is not a terminator
	BTEST_EXPECT(bscn_str_is_empty(bscn_accept_new_line(&scn)));
}

BTEST(pos, until_new_line) {
	// The manual line splitting pattern, ending agnostic
	bscn_t scn = bscn_make_cstr("one\r\ntwo\nthree");

	BTEST_EXPECT(bscn_str_eq(bscn_accept_until(&scn, bscn_new_line), BSCN_STR("one")));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept_new_line(&scn)));

	BTEST_EXPECT(bscn_str_eq(bscn_accept_until(&scn, bscn_new_line), BSCN_STR("two")));
	BTEST_EXPECT(!bscn_str_is_empty(bscn_accept_new_line(&scn)));

	BTEST_EXPECT(bscn_str_eq(bscn_accept_until(&scn, bscn_new_line), BSCN_STR("three")));
	BTEST_EXPECT(bscn_is_done(&scn));
}

BTEST(pos, seek) {
	bscn_t scn = bscn_make_cstr("one\ntwo\nthree");

	bscn_next_line(&scn);
	bscn_pos_t saved = bscn_pos(&scn);

	bscn_next_line(&scn);
	bscn_accept_str(&scn, BSCN_STR("th"));
	expect_pos(bscn_pos(&scn), 10, 3, 3);

	// Seek backwards to a saved position
	bscn_seek(&scn, saved);
	expect_pos(bscn_pos(&scn), 4, 2, 1);
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("two")));

	bscn_reset(&scn);
	expect_pos(bscn_pos(&scn), 0, 1, 1);
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&scn), BSCN_STR("one")));
}
