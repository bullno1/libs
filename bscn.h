#ifndef BSCN_H
#define BSCN_H

/**
 * @file
 *
 * @brief Text scanner for hand-written parsers.
 *
 * This is inspired by [bx::Scanner](https://bkaradzic.github.io/posts/scanner/).
 *
 * The scanner never allocates and never copies.
 * All results are views (@ref bscn_str_t) into the original input and stay
 * valid for as long as the input does.
 *
 * There is a single cursor and it only moves when explicitly told to:
 *
 * * `bscn_accept*` moves it forward past matched text.
 * * `bscn_peek*` tests without moving.
 * * @ref bscn_seek and @ref bscn_reset move it explicitly.
 *
 * A failed match returns an empty view anchored at the cursor and does not
 * move it.
 * "Matched nothing" and "did not match" both return an empty view.
 * This is intentional: empty tokens are often valid (e.g: a URL without
 * a path).
 * Structural questions ("was the delimiter there?") should be answered
 * separately by accepting or peeking the delimiter itself.
 *
 * Line endings: both `\n` and `\r\n` are handled transparently by
 * @ref bscn_next_line and @ref bscn_accept_new_line.
 * However, since views are zero-copy, a view that spans multiple lines will
 * still contain the raw `\r` bytes.
 * Prefer @ref bscn_next_line for content comparison, or clean up with
 * @ref bscn_str_trim.
 *
 * A scanner can be bounded to a part of a document (a "sub-scanner") while
 * still reporting positions in the coordinates of the whole document:
 *
 * * @ref bscn_begin_sub / @ref bscn_end_sub delimit a span with the cursor.
 * * @ref bscn_make_at pairs a view with a previously captured position.
 *
 * Example:
 *
 * @snippet samples/bscn.c bscn_kv
 */

#include <stdbool.h>
#include <stddef.h>

#ifndef BSCN_API
#define BSCN_API
#endif

/*! Customizable index/length type. Signed by default. */
#ifndef BSCN_INDEX_TYPE
#define BSCN_INDEX_TYPE int
#endif

#ifndef BSCN_ASSERT
#include <assert.h>
#define BSCN_ASSERT(cond) assert(cond)
#endif

typedef BSCN_INDEX_TYPE bscn_index_t;

/*! Non-owning view into the scanner's input. Not NUL-terminated. */
typedef struct {
	const char* chars;
	bscn_index_t len;
} bscn_str_t;

/*! Make a view from a string literal */
#define BSCN_STR(LIT) \
	((bscn_str_t){ .chars = LIT, .len = (bscn_index_t)(sizeof(LIT) - 1) })

/*! Character class predicate */
typedef bool (*bscn_char_predicate_t)(char ch);

/*!
 * @brief Position of the cursor.
 *
 * Line and column are 1-based.
 * The column is counted in bytes: tabs count as one and UTF-8 sequences count
 * as their byte length.
 */
typedef struct {
	/*! 0-based byte offset into the input */
	bscn_index_t offset;
	/*! 1-based line number */
	bscn_index_t line;
	/*! 1-based byte column within the line */
	bscn_index_t column;
} bscn_pos_t;

/*! Scanner state. Initialize with @ref bscn_make. */
typedef struct {
	bscn_str_t input;

	// Private, do not initialize
	bscn_index_t bscn__cursor;
	bscn_index_t bscn__line;
	bscn_index_t bscn__line_start;
	bscn_pos_t bscn__base;
} bscn_t;

#ifdef __cplusplus
extern "C" {
#endif

// Construction

/*! Create a scanner over an input view */
BSCN_API bscn_t
bscn_make(bscn_str_t input);

/*! Create a scanner over a NUL-terminated string */
BSCN_API bscn_t
bscn_make_cstr(const char* cstr);

/**
 * @brief Create a scanner whose reported positions start at `base`.
 *
 * Use this when `view` was cut out of a larger document and its original
 * position is known.
 * This is the analogue of the C preprocessor's `#line` directive.
 *
 * A common pattern is to pair it with a view-returning accept:
 *
 * @snippet samples/bscn.c bscn_kv
 *
 * @param view The input of the new scanner.
 * @param base The position of the first byte of `view` in the larger document.
 */
BSCN_API bscn_t
bscn_make_at(bscn_str_t view, bscn_pos_t base);

/**
 * @brief Mark the start of a sub-scanner at the current cursor.
 *
 * Pair with @ref bscn_end_sub.
 * The returned position can also be used for error reporting.
 */
BSCN_API bscn_pos_t
bscn_begin_sub(const bscn_t* scn);

/**
 * @brief Create a sub-scanner over everything consumed since `begin`.
 *
 * The sub-scanner reports positions in the parent's coordinate system.
 * Its input is the span from `begin` up to (but not including) the parent's
 * cursor, so end it *before* accepting a closing delimiter:
 *
 * @snippet samples/bscn.c bscn_sub
 *
 * The parent and the sub-scanner are independent after this call.
 *
 * @param scn The parent scanner.
 * @param begin A position previously returned by @ref bscn_begin_sub
 *   on the same scanner.
 */
BSCN_API bscn_t
bscn_end_sub(const bscn_t* scn, bscn_pos_t begin);

// State

/*! Whether the cursor is at the end of input */
BSCN_API bool
bscn_is_done(const bscn_t* scn);

/*! Current position of the cursor */
BSCN_API bscn_pos_t
bscn_pos(const bscn_t* scn);

/*! Everything from the cursor to the end of input */
BSCN_API bscn_str_t
bscn_remaining(const bscn_t* scn);

/**
 * @brief Move the cursor to a previously captured position.
 *
 * `pos` must have been returned by @ref bscn_pos or @ref bscn_begin_sub on
 * this same scanner.
 */
BSCN_API void
bscn_seek(bscn_t* scn, bscn_pos_t pos);

/*! Move the cursor back to the start of input */
BSCN_API void
bscn_reset(bscn_t* scn);

// Matching
//
// All matchers return a view of the matched text.
// accept* moves the cursor past the match; peek* never moves it.
// On failure, an empty view anchored at the cursor is returned and the cursor
// does not move.

/*! Consume a single expected character */
BSCN_API bscn_str_t
bscn_accept_char(bscn_t* scn, char ch);

/*! Consume an exact sequence (all or nothing) */
BSCN_API bscn_str_t
bscn_accept_str(bscn_t* scn, bscn_str_t str);

/*! Consume a (possibly empty) run of characters matching a predicate */
BSCN_API bscn_str_t
bscn_accept_while(bscn_t* scn, bscn_char_predicate_t pred);

/**
 * @brief Consume up to (not including) the first occurrence of a character.
 *
 * If the character never occurs, the rest of the input is consumed.
 */
BSCN_API bscn_str_t
bscn_accept_until_char(bscn_t* scn, char ch);

/**
 * @brief Consume up to (not including) the first occurrence of a sequence.
 *
 * If the sequence never occurs, the rest of the input is consumed.
 */
BSCN_API bscn_str_t
bscn_accept_until_str(bscn_t* scn, bscn_str_t str);

/**
 * @brief Consume up to (not including) the first character matching a predicate.
 *
 * If no character matches, the rest of the input is consumed.
 */
BSCN_API bscn_str_t
bscn_accept_until_pred(bscn_t* scn, bscn_char_predicate_t pred);

/*! Non-moving counterpart of @ref bscn_accept_char */
BSCN_API bscn_str_t
bscn_peek_char(const bscn_t* scn, char ch);

/*! Non-moving counterpart of @ref bscn_accept_str */
BSCN_API bscn_str_t
bscn_peek_str(const bscn_t* scn, bscn_str_t str);

/*! Non-moving counterpart of @ref bscn_accept_while */
BSCN_API bscn_str_t
bscn_peek_while(const bscn_t* scn, bscn_char_predicate_t pred);

/*! Consume one line terminator: `\r\n` or `\n`. Empty view if not at one. */
BSCN_API bscn_str_t
bscn_accept_new_line(bscn_t* scn);

/**
 * @brief Consume and return the next line, excluding the terminator.
 *
 * Both `\n` and `\r\n` are handled.
 * A blank line is a valid (empty) result, so loop on `!bscn_is_done(scn)`,
 * not on emptiness:
 *
 * @snippet samples/bscn.c bscn_line_loop
 */
BSCN_API bscn_str_t
bscn_next_line(bscn_t* scn);

// Character classes

/*! Whether `ch` is one of: space, `\t`, `\r`, `\n`, `\v`, `\f` */
BSCN_API bool
bscn_space(char ch);

/*! Negation of @ref bscn_space */
BSCN_API bool
bscn_non_space(char ch);

/*! Whether `ch` is `\r` or `\n` */
BSCN_API bool
bscn_new_line(char ch);

/*! Whether `ch` is a decimal digit */
BSCN_API bool
bscn_digit(char ch);

/*! Whether `ch` is in `[A-Za-z0-9_]` */
BSCN_API bool
bscn_identifier(char ch);

// View helpers

/*! Whether a view is empty. Both "no match" and "matched nothing" are empty. */
BSCN_API bool
bscn_str_is_empty(bscn_str_t str);

/*! Content equality */
BSCN_API bool
bscn_str_eq(bscn_str_t lhs, bscn_str_t rhs);

/*! Trim characters matching a predicate from both ends */
BSCN_API bscn_str_t
bscn_str_trim(bscn_str_t str, bscn_char_predicate_t pred);

// Adapters for the generic macros, do not call directly

BSCN_API bscn_str_t
bscn__accept_lit(bscn_t* scn, const char* lit);

BSCN_API bscn_str_t
bscn__accept_until_lit(bscn_t* scn, const char* lit);

BSCN_API bscn_str_t
bscn__peek_lit(const bscn_t* scn, const char* lit);

#ifdef __cplusplus
}
#endif

/**
 * @brief Generic version of the `bscn_accept_*` functions.
 *
 * Dispatch on the second argument:
 *
 * * `bscn_accept(scn, ';')` calls @ref bscn_accept_char.
 * * `bscn_accept(scn, "://")` calls @ref bscn_accept_str.
 * * `bscn_accept(scn, bscn_space)` calls @ref bscn_accept_while.
 *
 * @hideinitializer
 */
#define bscn_accept(SCN, WHAT) \
	_Generic((WHAT), \
		char: bscn_accept_char, \
		int: bscn_accept_char, \
		char*: bscn__accept_lit, \
		const char*: bscn__accept_lit, \
		bscn_str_t: bscn_accept_str, \
		bscn_char_predicate_t: bscn_accept_while \
	)((SCN), (WHAT))

/**
 * @brief Generic version of the `bscn_accept_until_*` functions.
 *
 * Dispatch on the second argument like @ref bscn_accept.
 *
 * @hideinitializer
 */
#define bscn_accept_until(SCN, WHAT) \
	_Generic((WHAT), \
		char: bscn_accept_until_char, \
		int: bscn_accept_until_char, \
		char*: bscn__accept_until_lit, \
		const char*: bscn__accept_until_lit, \
		bscn_str_t: bscn_accept_until_str, \
		bscn_char_predicate_t: bscn_accept_until_pred \
	)((SCN), (WHAT))

/**
 * @brief Generic version of the `bscn_peek_*` functions.
 *
 * Dispatch on the second argument like @ref bscn_accept.
 *
 * @hideinitializer
 */
#define bscn_peek(SCN, WHAT) \
	_Generic((WHAT), \
		char: bscn_peek_char, \
		int: bscn_peek_char, \
		char*: bscn__peek_lit, \
		const char*: bscn__peek_lit, \
		bscn_str_t: bscn_peek_str, \
		bscn_char_predicate_t: bscn_peek_while \
	)((SCN), (WHAT))

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSCN_IMPLEMENTATION)
#define BSCN_IMPLEMENTATION
#endif

#ifdef BSCN_IMPLEMENTATION

#include <string.h>

static bscn_str_t
bscn__empty(const bscn_t* scn) {
	return (bscn_str_t){
		.chars = scn->input.chars + scn->bscn__cursor,
		.len = 0,
	};
}

// The only place that moves the cursor forward.
// It maintains line tracking by counting newlines in the consumed span.
static bscn_str_t
bscn__advance(bscn_t* scn, bscn_index_t len) {
	bscn_str_t view = {
		.chars = scn->input.chars + scn->bscn__cursor,
		.len = len,
	};

	for (bscn_index_t i = 0; i < len; ++i) {
		if (view.chars[i] == '\n') {
			scn->bscn__line += 1;
			scn->bscn__line_start = scn->bscn__cursor + i + 1;
		}
	}
	scn->bscn__cursor += len;

	return view;
}

static bscn_str_t
bscn__cstr(const char* cstr) {
	size_t len = cstr != NULL ? strlen(cstr) : 0;
	bscn_index_t ilen = (bscn_index_t)len;
	BSCN_ASSERT(ilen >= 0 && (size_t)ilen == len && "Input too large for bscn_index_t");
	return (bscn_str_t){ .chars = cstr, .len = ilen };
}

bscn_t
bscn_make(bscn_str_t input) {
	return bscn_make_at(input, (bscn_pos_t){ .offset = 0, .line = 1, .column = 1 });
}

bscn_t
bscn_make_cstr(const char* cstr) {
	return bscn_make(bscn__cstr(cstr));
}

bscn_t
bscn_make_at(bscn_str_t view, bscn_pos_t base) {
	BSCN_ASSERT(view.len >= 0);
	BSCN_ASSERT(base.line >= 1 && base.column >= 1);
	return (bscn_t){
		.input = view,
		.bscn__cursor = 0,
		.bscn__line = 1,
		.bscn__line_start = 0,
		.bscn__base = base,
	};
}

bscn_pos_t
bscn_begin_sub(const bscn_t* scn) {
	return bscn_pos(scn);
}

bscn_t
bscn_end_sub(const bscn_t* scn, bscn_pos_t begin) {
	bscn_index_t begin_cursor = begin.offset - scn->bscn__base.offset;
	BSCN_ASSERT(0 <= begin_cursor && begin_cursor <= scn->bscn__cursor);

	bscn_str_t view = {
		.chars = scn->input.chars + begin_cursor,
		.len = scn->bscn__cursor - begin_cursor,
	};
	return bscn_make_at(view, begin);
}

bool
bscn_is_done(const bscn_t* scn) {
	return scn->bscn__cursor >= scn->input.len;
}

bscn_pos_t
bscn_pos(const bscn_t* scn) {
	bscn_index_t local_line = scn->bscn__line;
	bscn_index_t local_column = scn->bscn__cursor - scn->bscn__line_start + 1;
	bscn_pos_t base = scn->bscn__base;

	return (bscn_pos_t){
		.offset = base.offset + scn->bscn__cursor,
		.line = base.line + (local_line - 1),
		// Only the first line of the view is offset by where the view began
		.column = local_line == 1 ? base.column + (local_column - 1) : local_column,
	};
}

bscn_str_t
bscn_remaining(const bscn_t* scn) {
	return (bscn_str_t){
		.chars = scn->input.chars + scn->bscn__cursor,
		.len = scn->input.len - scn->bscn__cursor,
	};
}

void
bscn_seek(bscn_t* scn, bscn_pos_t pos) {
	bscn_pos_t base = scn->bscn__base;

	bscn_index_t cursor = pos.offset - base.offset;
	BSCN_ASSERT(0 <= cursor && cursor <= scn->input.len);

	bscn_index_t local_line = pos.line - base.line + 1;
	BSCN_ASSERT(local_line >= 1);

	bscn_index_t local_column = local_line == 1
		? pos.column - base.column + 1
		: pos.column;
	BSCN_ASSERT(1 <= local_column && local_column <= cursor + 1);

	scn->bscn__cursor = cursor;
	scn->bscn__line = local_line;
	scn->bscn__line_start = cursor - (local_column - 1);
}

void
bscn_reset(bscn_t* scn) {
	scn->bscn__cursor = 0;
	scn->bscn__line = 1;
	scn->bscn__line_start = 0;
}

bscn_str_t
bscn_accept_char(bscn_t* scn, char ch) {
	if (!bscn_is_done(scn) && scn->input.chars[scn->bscn__cursor] == ch) {
		return bscn__advance(scn, 1);
	} else {
		return bscn__empty(scn);
	}
}

bscn_str_t
bscn_accept_str(bscn_t* scn, bscn_str_t str) {
	if (bscn_str_is_empty(bscn_peek_str(scn, str))) {
		return bscn__empty(scn);
	} else {
		return bscn__advance(scn, str.len);
	}
}

bscn_str_t
bscn_accept_while(bscn_t* scn, bscn_char_predicate_t pred) {
	bscn_index_t i = scn->bscn__cursor;
	while (i < scn->input.len && pred(scn->input.chars[i])) { ++i; }
	return bscn__advance(scn, i - scn->bscn__cursor);
}

bscn_str_t
bscn_accept_until_char(bscn_t* scn, char ch) {
	bscn_index_t i = scn->bscn__cursor;
	while (i < scn->input.len && scn->input.chars[i] != ch) { ++i; }
	return bscn__advance(scn, i - scn->bscn__cursor);
}

bscn_str_t
bscn_accept_until_str(bscn_t* scn, bscn_str_t str) {
	if (str.len <= 0) { return bscn__empty(scn); }

	for (bscn_index_t i = scn->bscn__cursor; i + str.len <= scn->input.len; ++i) {
		if (memcmp(scn->input.chars + i, str.chars, (size_t)str.len) == 0) {
			return bscn__advance(scn, i - scn->bscn__cursor);
		}
	}

	return bscn__advance(scn, scn->input.len - scn->bscn__cursor);
}

bscn_str_t
bscn_accept_until_pred(bscn_t* scn, bscn_char_predicate_t pred) {
	bscn_index_t i = scn->bscn__cursor;
	while (i < scn->input.len && !pred(scn->input.chars[i])) { ++i; }
	return bscn__advance(scn, i - scn->bscn__cursor);
}

bscn_str_t
bscn_peek_char(const bscn_t* scn, char ch) {
	if (!bscn_is_done(scn) && scn->input.chars[scn->bscn__cursor] == ch) {
		return (bscn_str_t){
			.chars = scn->input.chars + scn->bscn__cursor,
			.len = 1,
		};
	} else {
		return bscn__empty(scn);
	}
}

bscn_str_t
bscn_peek_str(const bscn_t* scn, bscn_str_t str) {
	if (
		str.len > 0
		&& str.len <= scn->input.len - scn->bscn__cursor
		&& memcmp(scn->input.chars + scn->bscn__cursor, str.chars, (size_t)str.len) == 0
	) {
		return (bscn_str_t){
			.chars = scn->input.chars + scn->bscn__cursor,
			.len = str.len,
		};
	} else {
		return bscn__empty(scn);
	}
}

bscn_str_t
bscn_peek_while(const bscn_t* scn, bscn_char_predicate_t pred) {
	bscn_index_t i = scn->bscn__cursor;
	while (i < scn->input.len && pred(scn->input.chars[i])) { ++i; }
	return (bscn_str_t){
		.chars = scn->input.chars + scn->bscn__cursor,
		.len = i - scn->bscn__cursor,
	};
}

bscn_str_t
bscn_accept_new_line(bscn_t* scn) {
	const char* cur = scn->input.chars + scn->bscn__cursor;
	bscn_index_t remaining = scn->input.len - scn->bscn__cursor;

	if (remaining >= 2 && cur[0] == '\r' && cur[1] == '\n') {
		return bscn__advance(scn, 2);
	} else if (remaining >= 1 && cur[0] == '\n') {
		return bscn__advance(scn, 1);
	} else {
		return bscn__empty(scn);
	}
}

bscn_str_t
bscn_next_line(bscn_t* scn) {
	const char* chars = scn->input.chars;
	bscn_index_t begin = scn->bscn__cursor;

	bscn_index_t i = begin;
	while (i < scn->input.len && chars[i] != '\n') { ++i; }

	bscn_index_t content_end = i;
	if (content_end > begin && chars[content_end - 1] == '\r') { --content_end; }

	// Consume the terminator but exclude it from the view
	bscn_index_t consumed = i < scn->input.len ? i + 1 - begin : i - begin;
	bscn__advance(scn, consumed);

	return (bscn_str_t){ .chars = chars + begin, .len = content_end - begin };
}

bool
bscn_space(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool
bscn_non_space(char ch) {
	return !bscn_space(ch);
}

bool
bscn_new_line(char ch) {
	return ch == '\r' || ch == '\n';
}

bool
bscn_digit(char ch) {
	return '0' <= ch && ch <= '9';
}

bool
bscn_identifier(char ch) {
	return ('a' <= ch && ch <= 'z')
		|| ('A' <= ch && ch <= 'Z')
		|| bscn_digit(ch)
		|| ch == '_';
}

bool
bscn_str_is_empty(bscn_str_t str) {
	return str.len <= 0;
}

bool
bscn_str_eq(bscn_str_t lhs, bscn_str_t rhs) {
	if (lhs.len != rhs.len) { return false; }
	if (lhs.len <= 0) { return true; }
	return memcmp(lhs.chars, rhs.chars, (size_t)lhs.len) == 0;
}

bscn_str_t
bscn_str_trim(bscn_str_t str, bscn_char_predicate_t pred) {
	bscn_index_t begin = 0;
	bscn_index_t end = str.len;
	while (begin < end && pred(str.chars[begin])) { ++begin; }
	while (end > begin && pred(str.chars[end - 1])) { --end; }
	return (bscn_str_t){ .chars = str.chars + begin, .len = end - begin };
}

bscn_str_t
bscn__accept_lit(bscn_t* scn, const char* lit) {
	return bscn_accept_str(scn, bscn__cstr(lit));
}

bscn_str_t
bscn__accept_until_lit(bscn_t* scn, const char* lit) {
	return bscn_accept_until_str(scn, bscn__cstr(lit));
}

bscn_str_t
bscn__peek_lit(const bscn_t* scn, const char* lit) {
	return bscn_peek_str(scn, bscn__cstr(lit));
}

#endif
