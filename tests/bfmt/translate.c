#include "shared.h"
#include <limits.h>

// Locale {{{

// What the test locales answer with and what they were asked
static struct {
	const char* translation;  // Returned for every key, NULL means no translation
	bool drop_on_error;  // Forget the translation when it is reported as broken

	int num_lookups;
	const char* key;
	const char* ctx;

	int num_errors;
	int error_offset;
	const char* error_key;
	const char* error_ctx;
	const char* error_template;
} lookup;

static const char*
translate(void* userdata, const char* ctx, const char* key) {
	(void)userdata;
	lookup.num_lookups += 1;
	lookup.ctx = ctx;
	lookup.key = key;
	return lookup.translation;
}

static void
report_error(void* userdata, const char* ctx, const char* key, const char* template, int offset) {
	(void)userdata;
	lookup.num_errors += 1;
	lookup.error_offset = offset;
	lookup.error_ctx = ctx;
	lookup.error_key = key;
	lookup.error_template = template;
	if (lookup.drop_on_error) { lookup.translation = NULL; }
}

static bfmt_plural_t
english_rule(bfmt_plural_num_t n, bfmt_plural_type_t type) {
	if (type == BFMT_PLURAL_CARDINAL) {
		return (n.i == 1 && n.v == 0) ? BFMT_PLURAL_ONE : BFMT_PLURAL_OTHER;
	}

	uint64_t mod10 = n.i % 10;
	uint64_t mod100 = n.i % 100;
	if (mod10 == 1 && mod100 != 11) { return BFMT_PLURAL_ONE; }
	if (mod10 == 2 && mod100 != 12) { return BFMT_PLURAL_TWO; }
	if (mod10 == 3 && mod100 != 13) { return BFMT_PLURAL_FEW; }
	return BFMT_PLURAL_OTHER;
}

static bfmt_plural_t
polish_rule(bfmt_plural_num_t n, bfmt_plural_type_t type) {
	if (type == BFMT_PLURAL_ORDINAL || n.v != 0) { return BFMT_PLURAL_OTHER; }
	if (n.i == 1) { return BFMT_PLURAL_ONE; }

	uint64_t mod10 = n.i % 10;
	uint64_t mod100 = n.i % 100;
	if (mod10 >= 2 && mod10 <= 4 && !(mod100 >= 12 && mod100 <= 14)) { return BFMT_PLURAL_FEW; }
	return BFMT_PLURAL_MANY;
}

static bfmt_locale_t english = {
	.plural_rule = english_rule,
	.translate = translate,
	.report_error = report_error,
};

static bfmt_locale_t polish = {
	.plural_rule = polish_rule,
	.translate = translate,
	.report_error = report_error,
};

// }}}

static void
init_per_test(void) {
	init_output();
	memset(&lookup, 0, sizeof(lookup));
}

static btest_suite_t translate_suite = {
	.name = "bfmt/translate",
	.init_per_test = init_per_test,
};

// Render the inputs through `TEMPLATE` as if it was the translation
#define TRANSLATE_AS(LOCALE, TEMPLATE, ...) \
	do { \
		lookup.translation = TEMPLATE; \
		bfmt_translate(OUT, LOCALE, __VA_ARGS__); \
	} while (0)

#define EXPECT_STR(ACTUAL, EXPECTED) \
	BTEST_EXPECT_EX((ACTUAL) != NULL && strcmp(ACTUAL, EXPECTED) == 0, "got \"%s\"", (ACTUAL) != NULL ? (ACTUAL) : "(null)")

#define EXPECT_NO_ERROR() \
	BTEST_EXPECT_EQUAL("%d", lookup.num_errors, 0)

// The output must show the problem and it must be reported exactly once
#define EXPECT_ERROR(OUTPUT, OFFSET) \
	do { \
		EXPECT_OUTPUT(OUTPUT); \
		BTEST_EXPECT_EQUAL("%d", lookup.num_errors, 1); \
		BTEST_EXPECT_EQUAL("%d", lookup.error_offset, OFFSET); \
		lookup.num_errors = 0; \
	} while (0)

// Prints a count through a nested translation
bfmt_decl_formatter(int, bfmt_test_format_file_count, bfmt_no_options_t)

bfmt_formatter(int, bfmt_test_format_file_count, bfmt_no_options_t) {
	(void)options;
	bfmt_translate(ctx, &english, bfmt_plural(n, value, (one, "# file"), (other, "# files")));
}

// Source text {{{

BTEST(translate_suite, plain_text) {
	bfmt_translate(OUT, &english, "Hello");
	EXPECT_OUTPUT("Hello");

	bfmt_translate(OUT, &english, "Hello", ", ", "world");
	EXPECT_OUTPUT("Hello, world");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, named) {
	bfmt_translate(OUT, &english, "Hello ", bfmt_named(user, "bull"), ", you are ", bfmt_named(age, 30));
	EXPECT_OUTPUT("Hello bull, you are 30");

	bfmt_translate(OUT, &english, bfmt_named(value, bfmt(255, { .base = BFMT_BASE_HEX })));
	EXPECT_OUTPUT("ff");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, plural) {
	bfmt_translate(OUT, &english, "Copied ", bfmt_plural(num_files, 1, (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("Copied 1 file");

	bfmt_translate(OUT, &english, "Copied ", bfmt_plural(num_files, 5, (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("Copied 5 files");

	bfmt_translate(OUT, &english, bfmt_plural(n, 0, (=0, "nothing"), (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("nothing");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, ordinal) {
	for (int i = 1; i <= 4; ++i) {
		bfmt_translate(OUT, &english, bfmt_ordinal(n, i, (one, "#st"), (two, "#nd"), (few, "#rd"), (other, "#th")), " ");
	}
	EXPECT_OUTPUT("1st 2nd 3rd 4th ");

	bfmt_translate(OUT, &english, bfmt_ordinal(n, 11, (one, "#st"), (two, "#nd"), (few, "#rd"), (other, "#th")));
	EXPECT_OUTPUT("11th");

	bfmt_translate(OUT, &english, bfmt_ordinal(n, 22, (one, "#st"), (two, "#nd"), (few, "#rd"), (other, "#th")));
	EXPECT_OUTPUT("22nd");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, select) {
	bfmt_translate(OUT, &english, bfmt_select(gender, "female", (female, "her"), (male, "him"), (other, "them")));
	EXPECT_OUTPUT("her");

	bfmt_translate(OUT, &english, bfmt_select(gender, "male", (female, "her"), (male, "him"), (other, "them")));
	EXPECT_OUTPUT("him");

	bfmt_translate(OUT, &english, bfmt_select(gender, "unknown", (female, "her"), (male, "him"), (other, "them")));
	EXPECT_OUTPUT("them");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, inputs_are_evaluated_once) {
	int counter = 1;
	bfmt_translate(OUT, &english, bfmt_plural(n, counter++, (one, "# file"), (other, "# files")));
	BTEST_EXPECT_EQUAL("%d", counter, 2);
	EXPECT_OUTPUT("1 file");

	bfmt_translate(OUT, &english, bfmt_named(n, counter++));
	BTEST_EXPECT_EQUAL("%d", counter, 3);
	EXPECT_OUTPUT("2");
}

BTEST(translate_suite, without_a_locale) {
	// No translation and no plural rule: everything is `other`
	bfmt_translate(OUT, NULL, bfmt_plural(n, 1, (one, "# file"), (other, "# files")), " for ", bfmt_named(user, "bull"));
	EXPECT_OUTPUT("1 files for bull");

	bfmt_locale_t empty = { 0 };
	bfmt_translate(OUT, &empty, bfmt_plural(n, 1, (=1, "exactly one"), (other, "# files")));
	EXPECT_OUTPUT("exactly one");

	// A broken template without anyone to report to is still shown
	bfmt_translate(OUT, &empty, "{missing}");
	EXPECT_OUTPUT("{missing}");
}

BTEST(translate_suite, inside_a_formatter) {
	bfmt_print(OUT, "Copied ", bfmt_with(bfmt_test_format_file_count, 1));
	EXPECT_OUTPUT("Copied 1 file");

	bfmt_translate(OUT, &english, "Copied ", bfmt_named(what, bfmt_with(bfmt_test_format_file_count, 2)));
	EXPECT_OUTPUT("Copied 2 files");
	EXPECT_NO_ERROR();
}

// }}}

// Lookup {{{

// The generated template is the catalog key, so its exact shape matters
BTEST(translate_suite, lookup_key) {
	bfmt_translate(OUT, &english, "Copied ", bfmt_plural(num_files, 5, (one, "# file"), (other, "# files")), " for ", bfmt_named(user, "bull"));
	EXPECT_STR(lookup.key, "Copied {num_files, plural, one{# file} other{# files}} for {user}");

	bfmt_translate(OUT, &english, bfmt_plural(n, 0, (=0, "none"), (other, "#")));
	EXPECT_STR(lookup.key, "{n, plural, =0{none} other{#}}");

	bfmt_translate(OUT, &english, bfmt_ordinal(n, 1, (one, "#st"), (other, "#th")));
	EXPECT_STR(lookup.key, "{n, selectordinal, one{#st} other{#th}}");

	bfmt_translate(OUT, &english, bfmt_select(gender, "male", (male, "him"), (other, "them")));
	EXPECT_STR(lookup.key, "{gender, select, male{him} other{them}}");

	bfmt_translate(OUT, &english, "plain");
	EXPECT_STR(lookup.key, "plain");

	BTEST_EXPECT_EQUAL("%d", lookup.num_lookups, 5);
}

BTEST(translate_suite, context) {
	bfmt_translate(OUT, &english, "Open");
	BTEST_EXPECT(lookup.ctx == NULL);
	EXPECT_STR(lookup.key, "Open");

	bfmt_ptranslate(OUT, &english, "menu", "Open");
	EXPECT_STR(lookup.ctx, "menu");
	EXPECT_STR(lookup.key, "Open");

	lookup.translation = "Otwórz";
	bfmt_ptranslate(OUT, &polish, "menu", "Open");
	EXPECT_OUTPUT("OpenOpenOtwórz");
}

BTEST(translate_suite, static_text) {
	bfmt_text_t plain = bfmt_text("Save");
	BTEST_EXPECT(plain.context == NULL);
	EXPECT_STR(plain.content, "Save");

	bfmt_text_t with_context = bfmt_ptext("menu", "Open ", bfmt_named(file, 0));
	EXPECT_STR(with_context.context, "menu");
	EXPECT_STR(with_context.content, "Open {file}");

	bfmt_translate_text(OUT, &english, &plain);
	EXPECT_OUTPUT("Save");
	BTEST_EXPECT(lookup.ctx == NULL);
	EXPECT_STR(lookup.key, "Save");

	bfmt_text_t menu_open = bfmt_ptext("menu", "Open");
	lookup.translation = "Otwórz";
	bfmt_translate_text(OUT, &polish, &menu_open);
	EXPECT_OUTPUT("Otwórz");
	EXPECT_STR(lookup.ctx, "menu");
	EXPECT_STR(lookup.key, "Open");
}

BTEST(translate_suite, translation_reorders_inputs) {
	TRANSLATE_AS(
		&polish,
		"Dla {user} skopiowano {num_files, plural, one{# plik} few{# pliki} many{# plików} other{# pliku}}",
		"Copied ", bfmt_plural(num_files, 3, (one, "# file"), (other, "# files")), " for ", bfmt_named(user, "bull")
	);
	EXPECT_OUTPUT("Dla bull skopiowano 3 pliki");

	// An input may be used more than once, or not at all
	TRANSLATE_AS(&english, "{b}{b}", bfmt_named(a, 1), bfmt_named(b, 2));
	EXPECT_OUTPUT("22");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, plural_categories_follow_the_locale) {
	const char* translation = "{n, plural, one{# plik} few{# pliki} many{# plików} other{# pliku}}";
	int counts[] = { 1, 2, 4, 5, 12, 21, 22, 112 };
	for (int i = 0; i < (int)(sizeof(counts) / sizeof(counts[0])); ++i) {
		TRANSLATE_AS(&polish, translation, bfmt_plural(n, counts[i], (one, "# file"), (other, "# files")), ";");
	}
	EXPECT_OUTPUT("1 plik2 pliki4 pliki5 plików12 plików21 plików22 pliki112 plików");

	// The same source through the English rule
	lookup.translation = NULL;
	bfmt_translate(OUT, &english, bfmt_plural(n, 21, (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("21 files");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, translation_can_pluralize_a_named_input) {
	TRANSLATE_AS(
		&polish,
		"Masz {count, plural, one{# wiadomość} few{# wiadomości} many{# wiadomości} other{# wiadomości}}",
		"You have ", bfmt_named(count, 1), " messages"
	);
	EXPECT_OUTPUT("Masz 1 wiadomość");
	EXPECT_NO_ERROR();
}

// }}}

// Plural {{{

BTEST(translate_suite, explicit_value_beats_keyword) {
	TRANSLATE_AS(&english, "{n, plural, one{keyword} =1{explicit} other{other}}", bfmt_named(n, 1));
	EXPECT_OUTPUT("explicit");

	TRANSLATE_AS(&english, "{n, plural, =1{explicit} one{keyword} other{other}}", bfmt_named(n, 1));
	EXPECT_OUTPUT("explicit");

	TRANSLATE_AS(&english, "{n, plural, other{other} =2{explicit}}", bfmt_named(n, 2));
	EXPECT_OUTPUT("explicit");

	TRANSLATE_AS(&english, "{n, plural, =2{explicit} other{other}}", bfmt_named(n, 3));
	EXPECT_OUTPUT("other");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, every_integer_type) {
	const char* translation = "{n, plural, one{1} other{N}}";
	short s = 1;
	unsigned short us = 1;
	unsigned int u = 1;
	long l = 1;
	unsigned long ul = 1;
	long long ll = 1;
	unsigned long long ull = 1;
	bool b = true;
	char c = 1;

	TRANSLATE_AS(&english, translation, bfmt_named(n, s));
	TRANSLATE_AS(&english, translation, bfmt_named(n, us));
	TRANSLATE_AS(&english, translation, bfmt_named(n, 1));
	TRANSLATE_AS(&english, translation, bfmt_named(n, u));
	TRANSLATE_AS(&english, translation, bfmt_named(n, l));
	TRANSLATE_AS(&english, translation, bfmt_named(n, ul));
	TRANSLATE_AS(&english, translation, bfmt_named(n, ll));
	TRANSLATE_AS(&english, translation, bfmt_named(n, ull));
	TRANSLATE_AS(&english, translation, bfmt_named(n, b));
	TRANSLATE_AS(&english, translation, bfmt_named(n, c));
	EXPECT_OUTPUT("1111111111");

	TRANSLATE_AS(&english, translation, bfmt_named(n, 2));
	TRANSLATE_AS(&english, translation, bfmt_named(n, 2ull));
	EXPECT_OUTPUT("NN");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, negative_and_extreme_counts) {
	// The category comes from the absolute value, `=N` does not match a negative
	TRANSLATE_AS(&english, "{n, plural, =1{exact} one{# thing} other{# things}}", bfmt_named(n, -1));
	EXPECT_OUTPUT("-1 thing");

	TRANSLATE_AS(&english, "{n, plural, one{# thing} other{# things}}", bfmt_named(n, LLONG_MIN));
	EXPECT_OUTPUT("-9223372036854775808 things");

	TRANSLATE_AS(&english, "{n, plural, =18446744073709551615{max} other{#}}", bfmt_named(n, ULLONG_MAX));
	EXPECT_OUTPUT("max");

	TRANSLATE_AS(&english, "{n, plural, =0{zero} other{#}}", bfmt_named(n, 0u));
	EXPECT_OUTPUT("zero");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, hash_uses_the_formatting_options) {
	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(7, { .layout.min_width = 3, .zero_pad = true }), (one, "[#]"), (other, "[#]")));
	EXPECT_OUTPUT("[007]");

	// The count is still understood, 1 is `one`
	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(1, { .layout.min_width = 3 }), (one, "[#] file"), (other, "[#] files")));
	EXPECT_OUTPUT("[  1] file");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, visible_fraction_digits_select_the_category) {
	// "1.0" is not `one` in English, "1" is
	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(1.0, { .precision = bfmt_precision(1) }), (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("1.0 files");

	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(1.0, { .precision = bfmt_precision(0) }), (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("1 file");

	bfmt_translate(OUT, &english, bfmt_plural(n, 1.0, (one, "# file"), (other, "# files")));
	EXPECT_OUTPUT("1.000000 files");

	// `=N` compares the value: 1.00 is 1 but 1.5 is not
	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(1.0, { .precision = bfmt_precision(2) }), (=1, "exactly one"), (other, "#")));
	EXPECT_OUTPUT("exactly one");

	bfmt_translate(OUT, &english, bfmt_plural(n, bfmt(1.5, { .precision = bfmt_precision(1) }), (=1, "exactly one"), (other, "#")));
	EXPECT_OUTPUT("1.5");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, hash_scope) {
	// A select keeps the '#' of the plural around it
	TRANSLATE_AS(
		&english,
		"{n, plural, other{{g, select, f{her # things} other{their # things}}}}",
		bfmt_named(n, 3), bfmt_named(g, "f")
	);
	EXPECT_OUTPUT("her 3 things");

	// A nested plural has its own
	TRANSLATE_AS(&english, "{a, plural, other{# then {b, plural, other{#}} then #}}", bfmt_named(a, 1), bfmt_named(b, 2));
	EXPECT_OUTPUT("1 then 2 then 1");

	// Outside of a plural it is plain text
	TRANSLATE_AS(&english, "Issue #5, {g, select, other{still #}}", bfmt_named(g, "x"));
	EXPECT_OUTPUT("Issue #5, still #");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, offset_is_done_by_the_caller) {
	const char* translation = "{others, plural, =0{just {who}} one{{who} and # other} other{{who} and # others}}; ";
	for (int n = 1; n <= 3; ++n) {
		TRANSLATE_AS(&english, translation, bfmt_named(others, n - 1), bfmt_named(who, "Ann"));
	}
	EXPECT_OUTPUT("just Ann; Ann and 1 other; Ann and 2 others; ");
	EXPECT_NO_ERROR();

	TRANSLATE_AS(&english, "{n, plural, offset:1 other{#}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, offset:1 other{#}}", 21);
}

// }}}

// Select {{{

BTEST(translate_suite, select_on_strings) {
	const char* translation = "{g, select, male{him} other{them}}";
	char buf[] = "male";
	char* mutable = buf;
	const char* null = NULL;

	TRANSLATE_AS(&english, translation, bfmt_named(g, mutable));
	EXPECT_OUTPUT("him");

	TRANSLATE_AS(&english, translation, bfmt_named(g, null));
	EXPECT_OUTPUT("them");

	TRANSLATE_AS(&english, translation, bfmt_named(g, ""));
	EXPECT_OUTPUT("them");

	// The whole key must match
	TRANSLATE_AS(&english, "{g, select, mal{shorter} males{longer} other{neither}}", bfmt_named(g, mutable));
	EXPECT_OUTPUT("neither");

	// And it is case sensitive
	TRANSLATE_AS(&english, translation, bfmt_named(g, "Male"));
	EXPECT_OUTPUT("them");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, select_on_other_types) {
	// Anything that is not a string is matched by how it prints
	TRANSLATE_AS(&english, "{mode, select, 1{first} 2{second} other{neither}}", bfmt_named(mode, 2));
	EXPECT_OUTPUT("second");

	bool enabled = true;
	TRANSLATE_AS(&english, "{enabled, select, true{on} other{off}}", bfmt_named(enabled, enabled));
	EXPECT_OUTPUT("on");

	TRANSLATE_AS(&english, "{mode, select, ff{hex} other{no}}", bfmt_named(mode, bfmt(255, { .base = BFMT_BASE_HEX })));
	EXPECT_OUTPUT("hex");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, first_matching_case_wins) {
	TRANSLATE_AS(&english, "{g, select, a{first} a{second} other{x}}", bfmt_named(g, "a"));
	EXPECT_OUTPUT("first");

	TRANSLATE_AS(&english, "{g, select, other{first} other{second}}", bfmt_named(g, "a"));
	EXPECT_OUTPUT("first");
	EXPECT_NO_ERROR();
}

// }}}

// Syntax {{{

BTEST(translate_suite, whitespace) {
	TRANSLATE_AS(
		&english,
		"{ n , plural , one { # thing } other\n\t{ # things } }|{ user }|",
		bfmt_named(n, 3), bfmt_named(user, "bull")
	);
	EXPECT_OUTPUT(" 3 things |bull|");

	TRANSLATE_AS(&english, "{n,plural,one{#}other{#s}}", bfmt_named(n, 2));
	EXPECT_OUTPUT("2s");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, quoting) {
	TRANSLATE_AS(&english, "It''s '{'not'}' an argument, {n}", bfmt_named(n, 7));
	EXPECT_OUTPUT("It's {not} an argument, 7");

	// A whole span can be quoted
	TRANSLATE_AS(&english, "'{n} and }{' but {n}", bfmt_named(n, 7));
	EXPECT_OUTPUT("{n} and }{ but 7");

	// An apostrophe is only special before '{', '}' and '#'
	TRANSLATE_AS(&english, "don't, rock 'n' roll, {n}'s", bfmt_named(n, 7));
	EXPECT_OUTPUT("don't, rock 'n' roll, 7's");

	// Quotes around an argument have to be doubled
	TRANSLATE_AS(&english, "the ''{n}'' field", bfmt_named(n, 7));
	EXPECT_OUTPUT("the '7' field");

	// A doubled apostrophe inside quoted text does not end it
	TRANSLATE_AS(&english, "'{it''s}' {n}", bfmt_named(n, 7));
	EXPECT_OUTPUT("{it's} 7");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, quoting_hash) {
	TRANSLATE_AS(&english, "{n, plural, other{'#' is # here}}", bfmt_named(n, 7));
	EXPECT_OUTPUT("# is 7 here");

	// Unlike ICU, this is a quote outside of a plural too
	TRANSLATE_AS(&english, "'#' {n}", bfmt_named(n, 7));
	EXPECT_OUTPUT("# 7");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, quoting_inside_cases) {
	const char* translation = "{n, plural, one{a '}' b} other{c '{' d}} end";
	TRANSLATE_AS(&english, translation, bfmt_named(n, 1));
	EXPECT_OUTPUT("a } b end");
	TRANSLATE_AS(&english, translation, bfmt_named(n, 2));
	EXPECT_OUTPUT("c { d end");

	translation = "{n, plural, one{a '#{' b''s} other{c}} end";
	TRANSLATE_AS(&english, translation, bfmt_named(n, 1));
	EXPECT_OUTPUT("a #{ b's end");
	TRANSLATE_AS(&english, translation, bfmt_named(n, 2));
	EXPECT_OUTPUT("c end");
	EXPECT_NO_ERROR();
}

BTEST(translate_suite, values_are_not_parsed) {
	bfmt_translate(OUT, &english, "Hello ", bfmt_named(user, "{n, plural, '#}"));
	EXPECT_OUTPUT("Hello {n, plural, '#}");

	bfmt_translate(OUT, &english, bfmt_plural(n, 1, (one, "# from {who}"), (other, "#")), bfmt_named(who, "}'{"));
	EXPECT_OUTPUT("1 from }'{}'{");
	EXPECT_NO_ERROR();
}

// }}}

// Errors {{{

BTEST(translate_suite, error_report) {
	TRANSLATE_AS(&polish, "{nope}", "source ", bfmt_named(n, 1));
	EXPECT_ERROR("{nope}", 1);
	BTEST_EXPECT(lookup.error_ctx == NULL);
	EXPECT_STR(lookup.error_key, "source {n}");
	EXPECT_STR(lookup.error_template, "{nope}");

	lookup.translation = "{nope}";
	bfmt_ptranslate(OUT, &polish, "menu", "source");
	EXPECT_ERROR("{nope}", 1);
	EXPECT_STR(lookup.error_ctx, "menu");
}

BTEST(translate_suite, error_in_the_source) {
	// The template is the key itself when there is no translation
	bfmt_translate(OUT, &english, "{missing} and ", bfmt_named(n, 1));
	EXPECT_ERROR("{missing} and {n}", 1);
	BTEST_EXPECT(lookup.error_template == lookup.error_key);
	EXPECT_STR(lookup.error_key, "{missing} and {n}");
}

BTEST(translate_suite, text_before_an_error_is_kept) {
	TRANSLATE_AS(&english, "abc {x} def {nope} ghi", bfmt_named(x, "X"));
	EXPECT_ERROR("abc X def {nope} ghi", 13);

	TRANSLATE_AS(&english, "{x}{x}}{x}", bfmt_named(x, "X"));
	EXPECT_ERROR("XX}{x}", 6);
}

BTEST(translate_suite, syntax_errors) {
	// Missing `other`
	TRANSLATE_AS(&english, "{n, plural, one{x}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, one{x}}", 18);

	TRANSLATE_AS(&english, "{n, select, a{x}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, select, a{x}}", 16);

	// No cases at all
	TRANSLATE_AS(&english, "{n, plural, }", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, }", 12);

	// Unsupported type
	TRANSLATE_AS(&english, "{n, number}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, number}", 4);

	// `=N` is for plurals only, and has no space
	TRANSLATE_AS(&english, "{n, select, =1{x} other{y}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, select, =1{x} other{y}}", 12);

	TRANSLATE_AS(&english, "{n, plural, = 1{x} other{y}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, = 1{x} other{y}}", 13);

	// Missing pieces
	TRANSLATE_AS(&english, "{}", bfmt_named(n, 1));
	EXPECT_ERROR("{}", 1);

	TRANSLATE_AS(&english, "{n", bfmt_named(n, 1));
	EXPECT_ERROR("{n", 2);

	TRANSLATE_AS(&english, "{n, plural}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural}", 10);

	TRANSLATE_AS(&english, "{n, plural, other}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, other}", 17);

	TRANSLATE_AS(&english, "{n, plural, other{x}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, other{x}", 20);

	TRANSLATE_AS(&english, "{n, plural, other{x", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, other{x", 19);

	// Stray closing brace
	TRANSLATE_AS(&english, "oops }", bfmt_named(n, 1));
	EXPECT_ERROR("oops }", 5);

	// A quote that never ends swallows the closing braces
	TRANSLATE_AS(&english, "{n, plural, other{unterminated '{ quote}}", bfmt_named(n, 1));
	EXPECT_ERROR("{n, plural, other{unterminated '{ quote}}", 41);
}

BTEST(translate_suite, error_in_a_case) {
	const char* translation = "{n, plural, one{{nope}} other{fine}} tail";

	// Not noticed until the case is selected
	TRANSLATE_AS(&english, translation, bfmt_named(n, 5));
	EXPECT_OUTPUT("fine tail");
	EXPECT_NO_ERROR();

	// Nothing after the error is rendered twice
	TRANSLATE_AS(&english, translation, bfmt_named(n, 1));
	EXPECT_ERROR("{nope}} other{fine}} tail", 17);

	// The text of the case before the error is kept
	TRANSLATE_AS(&english, "{n, plural, other{# is {nope}}} tail", bfmt_named(n, 1));
	EXPECT_ERROR("1 is {nope}}} tail", 24);
}

BTEST(translate_suite, locale_can_drop_a_broken_translation) {
	lookup.drop_on_error = true;

	TRANSLATE_AS(&polish, "broken {", "Hello ", bfmt_named(user, "bull"));
	EXPECT_ERROR("broken {", 8);

	// The next lookup misses and the source is used
	bfmt_translate(OUT, &polish, "Hello ", bfmt_named(user, "bull"));
	EXPECT_OUTPUT("Hello bull");
	EXPECT_NO_ERROR();
}

// }}}
