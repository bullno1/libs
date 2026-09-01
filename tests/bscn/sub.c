#include "../../bscn.h"
#include "../../btest.h"

static btest_suite_t sub = {
	.name = "bscn/sub",
};

static void
expect_pos(bscn_pos_t actual, bscn_index_t offset, bscn_index_t line, bscn_index_t column) {
	BTEST_EXPECT_EQUAL("%d", (int)actual.offset, (int)offset);
	BTEST_EXPECT_EQUAL("%d", (int)actual.line, (int)line);
	BTEST_EXPECT_EQUAL("%d", (int)actual.column, (int)column);
}

BTEST(sub, begin_end) {
	bscn_t scn = bscn_make_cstr("head\nAA\nBB\ntail");

	bscn_next_line(&scn);

	bscn_pos_t begin = bscn_begin_sub(&scn);
	bscn_accept_until_str(&scn, BSCN_STR("tail"));
	bscn_t block = bscn_end_sub(&scn, begin);

	BTEST_EXPECT(bscn_str_eq(block.input, BSCN_STR("AA\nBB\n")));
	BTEST_EXPECT(bscn_str_eq(bscn_remaining(&scn), BSCN_STR("tail")));

	// The sub-scanner reports positions in the parent's coordinates
	expect_pos(bscn_pos(&block), 5, 2, 1);
	BTEST_EXPECT(bscn_str_eq(bscn_next_line(&block), BSCN_STR("AA")));
	expect_pos(bscn_pos(&block), 8, 3, 1);
	bscn_accept_char(&block, 'B');
	expect_pos(bscn_pos(&block), 9, 3, 2);

	// The parent is unaffected
	expect_pos(bscn_pos(&scn), 11, 4, 1);
}

BTEST(sub, empty) {
	bscn_t scn = bscn_make_cstr("ab");
	bscn_accept_char(&scn, 'a');

	bscn_t block = bscn_end_sub(&scn, bscn_begin_sub(&scn));
	BTEST_EXPECT(bscn_str_is_empty(block.input));
	BTEST_EXPECT(bscn_is_done(&block));
	expect_pos(bscn_pos(&block), 1, 1, 2);
}

BTEST(sub, make_at) {
	// A view with a known origin in a larger document
	bscn_t scn = bscn_make_at(
		BSCN_STR("ab\ncd"),
		(bscn_pos_t){ .offset = 100, .line = 10, .column = 5 }
	);

	expect_pos(bscn_pos(&scn), 100, 10, 5);

	// The first line is offset by the base column
	bscn_accept_str(&scn, BSCN_STR("ab"));
	expect_pos(bscn_pos(&scn), 102, 10, 7);

	// Later lines are not
	bscn_accept_new_line(&scn);
	expect_pos(bscn_pos(&scn), 103, 11, 1);
	bscn_accept_str(&scn, BSCN_STR("cd"));
	expect_pos(bscn_pos(&scn), 105, 11, 3);
}

BTEST(sub, make_at_line) {
	// Line-bound sub-scanner: pair bscn_next_line with the position
	// captured right before it
	bscn_t scn = bscn_make_cstr("key = value\nnext");
	bscn_pos_t line_pos = bscn_pos(&scn);
	bscn_t line = bscn_make_at(bscn_next_line(&scn), line_pos);

	bscn_str_t key = bscn_accept_while(&line, bscn_identifier);
	BTEST_EXPECT(bscn_str_eq(key, BSCN_STR("key")));
	bscn_accept_while(&line, bscn_space);
	bscn_accept_char(&line, '=');
	expect_pos(bscn_pos(&line), 5, 1, 6);

	// The sub-scanner cannot run past its line
	bscn_accept_until_char(&line, 'x');
	BTEST_EXPECT(bscn_is_done(&line));
	expect_pos(bscn_pos(&line), 11, 1, 12);
}

BTEST(sub, nested) {
	bscn_t scn = bscn_make_cstr("1\n2 [deep]\n3");

	bscn_next_line(&scn);

	// Outer sub: the second line
	bscn_pos_t outer_begin = bscn_begin_sub(&scn);
	bscn_accept_until_pred(&scn, bscn_new_line);
	bscn_t outer = bscn_end_sub(&scn, outer_begin);

	// Inner sub: the bracketed part
	bscn_accept_until_char(&outer, '[');
	bscn_accept_char(&outer, '[');
	bscn_pos_t inner_begin = bscn_begin_sub(&outer);
	bscn_accept_until_char(&outer, ']');
	bscn_t inner = bscn_end_sub(&outer, inner_begin);

	BTEST_EXPECT(bscn_str_eq(inner.input, BSCN_STR("deep")));
	expect_pos(bscn_pos(&inner), 5, 2, 4);
	bscn_accept_while(&inner, bscn_identifier);
	expect_pos(bscn_pos(&inner), 9, 2, 8);
}

BTEST(sub, seek_in_sub) {
	// Seeking works with positions in parent coordinates
	bscn_t scn = bscn_make_cstr("head [ab\ncd]");

	bscn_accept_until_char(&scn, '[');
	bscn_accept_char(&scn, '[');
	bscn_pos_t begin = bscn_begin_sub(&scn);
	bscn_accept_until_char(&scn, ']');
	bscn_t block = bscn_end_sub(&scn, begin);

	bscn_next_line(&block);
	bscn_pos_t saved = bscn_pos(&block);
	expect_pos(saved, 9, 2, 1);
	bscn_accept_str(&block, BSCN_STR("cd"));

	bscn_seek(&block, saved);
	expect_pos(bscn_pos(&block), 9, 2, 1);
	BTEST_EXPECT(bscn_str_eq(bscn_remaining(&block), BSCN_STR("cd")));
}
