#include <stdio.h>

#define BLIB_IMPLEMENTATION
#include "../bscn.h"

// Parse a document with an INI-like front matter followed by a free-form body:
//
//   ---
//   name = water
//   tint = deep blue
//   ---
//   The body follows the front matter.
//
// Positions reported from within the front matter sub-scanner are positions
// in the whole document, not in the extracted block.

#define PRISTR(STR) (int)(STR).len, (STR).chars

static void
parse_front_matter(bscn_t frontmatter) {
	// Bind a sub-scanner to each line so that a malformed line cannot be
	// parsed beyond its end
	//!                                                                     [bscn_kv]
	while (!bscn_is_done(&frontmatter)) {
		bscn_pos_t line_pos = bscn_pos(&frontmatter);
		bscn_t line = bscn_make_at(bscn_next_line(&frontmatter), line_pos);

		bscn_accept(&line, bscn_space);
		if (bscn_is_done(&line)) { continue; }  // Blank line
		if (!bscn_str_is_empty(bscn_accept(&line, ';'))) { continue; }  // Comment

		bscn_str_t key = bscn_accept(&line, bscn_identifier);
		bscn_accept(&line, bscn_space);
		if (bscn_str_is_empty(bscn_accept(&line, '='))) {
			// The error is reported in document coordinates
			bscn_pos_t error_pos = bscn_pos(&line);
			printf(
				"%d:%d: expected `key = value`\n",
				(int)error_pos.line, (int)error_pos.column
			);
			continue;
		}
		bscn_str_t value = bscn_str_trim(bscn_remaining(&line), bscn_space);

		printf("%.*s => %.*s\n", PRISTR(key), PRISTR(value));
	}
	//!                                                                     [bscn_kv]
}

static void
parse_document(bscn_str_t document) {
	bscn_t scn = bscn_make(document);

	// Extract the front matter between the "---" fences.
	// bscn_end_sub spans everything consumed since bscn_begin_sub, so the
	// block is ended before the closing fence is accepted.
	//!                                                                     [bscn_sub]
	bscn_accept(&scn, "---");
	bscn_accept_new_line(&scn);

	bscn_pos_t front_matter_begin = bscn_begin_sub(&scn);
	bscn_accept_until(&scn, "\n---");
	bscn_accept_new_line(&scn);
	bscn_t frontmatter = bscn_end_sub(&scn, front_matter_begin);

	bscn_accept(&scn, "---");
	bscn_accept_new_line(&scn);
	//!                                                                     [bscn_sub]

	parse_front_matter(frontmatter);

	// Print the body with document line numbers
	//!                                                                     [bscn_line_loop]
	while (!bscn_is_done(&scn)) {
		bscn_pos_t pos = bscn_pos(&scn);
		bscn_str_t body_line = bscn_next_line(&scn);
		printf("%3d| %.*s\n", (int)pos.line, PRISTR(body_line));
	}
	//!                                                                     [bscn_line_loop]
}

int
main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	parse_document(BSCN_STR(
		"---\n"
		"; a comment\n"
		"name = water\n"
		"tint = deep blue\n"
		"oops\n"
		"speed = 3\n"
		"---\n"
		"The body follows the front matter.\n"
		"It spans multiple lines.\n"
	));

	return 0;
}
