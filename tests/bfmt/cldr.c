#include "shared.h"
#include "../../bfmt_cldr.h"

static btest_suite_t cldr_suite = {
	.name = "bfmt/cldr",
	.init_per_test = init_output,
};

// A number from its integer part and its visible fraction digits
static bfmt_plural_num_t
num(uint64_t i, unsigned v, uint64_t f) {
	bfmt_plural_num_t n = { .i = i, .v = v, .f = f, .t = f, .w = v };
	while (n.w > 0 && n.t % 10 == 0) {
		n.t /= 10;
		n.w -= 1;
	}
	return n;
}

#define CARDINAL(LANG, N, CATEGORY) \
	BTEST_EXPECT_EQUAL("%d", (int)BFMT_PLURAL_##CATEGORY, (int)bfmt_cldr_plural_rule(LANG)(N, BFMT_PLURAL_CARDINAL))

#define ORDINAL(LANG, N, CATEGORY) \
	BTEST_EXPECT_EQUAL("%d", (int)BFMT_PLURAL_##CATEGORY, (int)bfmt_cldr_plural_rule(LANG)(N, BFMT_PLURAL_ORDINAL))

BTEST(cldr_suite, lookup) {
	BTEST_EXPECT(bfmt_cldr_plural_rule("en") != NULL);
	BTEST_EXPECT(bfmt_cldr_plural_rule("en") == bfmt_cldr_plural_rule("EN"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("en") == bfmt_cldr_plural_rule("en_US.UTF-8"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("en") == bfmt_cldr_plural_rule("en-GB"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("sr") == bfmt_cldr_plural_rule("sr@latin"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("sr") == bfmt_cldr_plural_rule("sr_Latn_RS"));

	// Languages with the same rules share one function
	BTEST_EXPECT(bfmt_cldr_plural_rule("de") == bfmt_cldr_plural_rule("nl"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("de") != bfmt_cldr_plural_rule("en"));

	BTEST_EXPECT(bfmt_cldr_plural_rule("") == NULL);
	BTEST_EXPECT(bfmt_cldr_plural_rule("-") == NULL);
	BTEST_EXPECT(bfmt_cldr_plural_rule("xx") == NULL);
	BTEST_EXPECT(bfmt_cldr_plural_rule("xx_YY") == NULL);
	BTEST_EXPECT(bfmt_cldr_plural_rule("a-very-long-tag-that-does-not-fit-the-buffer-at-all") == NULL);
}

BTEST(cldr_suite, region) {
	// CLDR distinguishes European Portuguese; Brazilian is the plain language
	BTEST_EXPECT(bfmt_cldr_plural_rule("pt") != bfmt_cldr_plural_rule("pt_PT"));
	BTEST_EXPECT(bfmt_cldr_plural_rule("pt") == bfmt_cldr_plural_rule("pt_BR"));
	CARDINAL("pt", num(0, 0, 0), ONE);
	CARDINAL("pt_PT", num(0, 0, 0), OTHER);
	CARDINAL("pt_PT", num(1, 0, 0), ONE);
}

BTEST(cldr_suite, english) {
	CARDINAL("en", num(1, 0, 0), ONE);
	CARDINAL("en", num(0, 0, 0), OTHER);
	CARDINAL("en", num(2, 0, 0), OTHER);
	CARDINAL("en", num(1, 1, 0), OTHER);  // 1.0
	CARDINAL("en", num(1, 1, 5), OTHER);  // 1.5

	ORDINAL("en", num(1, 0, 0), ONE);
	ORDINAL("en", num(2, 0, 0), TWO);
	ORDINAL("en", num(3, 0, 0), FEW);
	ORDINAL("en", num(4, 0, 0), OTHER);
	ORDINAL("en", num(11, 0, 0), OTHER);
	ORDINAL("en", num(12, 0, 0), OTHER);
	ORDINAL("en", num(13, 0, 0), OTHER);
	ORDINAL("en", num(21, 0, 0), ONE);
	ORDINAL("en", num(22, 0, 0), TWO);
	ORDINAL("en", num(23, 0, 0), FEW);
	ORDINAL("en", num(111, 0, 0), OTHER);
	ORDINAL("en", num(1, 1, 0), ONE);  // 1.0 is 1 as n
	ORDINAL("en", num(1, 1, 5), OTHER);  // 1.5
}

BTEST(cldr_suite, polish) {
	CARDINAL("pl", num(1, 0, 0), ONE);
	CARDINAL("pl", num(2, 0, 0), FEW);
	CARDINAL("pl", num(4, 0, 0), FEW);
	CARDINAL("pl", num(5, 0, 0), MANY);
	CARDINAL("pl", num(12, 0, 0), MANY);
	CARDINAL("pl", num(14, 0, 0), MANY);
	CARDINAL("pl", num(22, 0, 0), FEW);
	CARDINAL("pl", num(0, 0, 0), MANY);
	CARDINAL("pl", num(1, 1, 5), OTHER);  // 1.5
	ORDINAL("pl", num(1, 0, 0), OTHER);
}

BTEST(cldr_suite, russian) {
	CARDINAL("ru", num(1, 0, 0), ONE);
	CARDINAL("ru", num(21, 0, 0), ONE);
	CARDINAL("ru", num(11, 0, 0), MANY);
	CARDINAL("ru", num(2, 0, 0), FEW);
	CARDINAL("ru", num(5, 0, 0), MANY);
	CARDINAL("ru", num(0, 0, 0), MANY);
	CARDINAL("ru", num(1, 1, 5), OTHER);  // 1.5
}

BTEST(cldr_suite, arabic) {
	CARDINAL("ar", num(0, 0, 0), ZERO);
	CARDINAL("ar", num(1, 0, 0), ONE);
	CARDINAL("ar", num(2, 0, 0), TWO);
	CARDINAL("ar", num(3, 0, 0), FEW);
	CARDINAL("ar", num(10, 0, 0), FEW);
	CARDINAL("ar", num(11, 0, 0), MANY);
	CARDINAL("ar", num(99, 0, 0), MANY);
	CARDINAL("ar", num(100, 0, 0), OTHER);
	CARDINAL("ar", num(103, 0, 0), FEW);
	CARDINAL("ar", num(1, 1, 5), OTHER);  // 1.5
}

BTEST(cldr_suite, french) {
	CARDINAL("fr", num(0, 0, 0), ONE);
	CARDINAL("fr", num(1, 0, 0), ONE);
	CARDINAL("fr", num(1, 1, 5), ONE);  // 1.5
	CARDINAL("fr", num(2, 0, 0), OTHER);
	// The exponent is always 0 without compact notation, so only a full million is many
	CARDINAL("fr", num(1000000, 0, 0), MANY);
	CARDINAL("fr", num(2000000, 0, 0), MANY);
	CARDINAL("fr", num(1000000, 1, 0), OTHER);  // 1000000.0
	CARDINAL("fr", num(1000001, 0, 0), OTHER);
	ORDINAL("fr", num(1, 0, 0), ONE);
	ORDINAL("fr", num(2, 0, 0), OTHER);
}

BTEST(cldr_suite, japanese) {
	CARDINAL("ja", num(0, 0, 0), OTHER);
	CARDINAL("ja", num(1, 0, 0), OTHER);
	CARDINAL("ja", num(2, 0, 0), OTHER);
	ORDINAL("ja", num(1, 0, 0), OTHER);
}

BTEST(cldr_suite, with_bfmt) {
	bfmt_locale_t polish = { .plural_rule = bfmt_cldr_plural_rule("pl") };
	int counts[] = { 1, 2, 5 };
	for (int i = 0; i < 3; ++i) {
		bfmt_translate(
			OUT, &polish,
			bfmt_plural(n, counts[i], (one, "# plik"), (few, "# pliki"), (many, "# plików"), (other, "# pliku")),
			" "
		);
	}
	EXPECT_OUTPUT("1 plik 2 pliki 5 plików ");

	bfmt_locale_t english = { .plural_rule = bfmt_cldr_plural_rule("en") };
	for (int i = 1; i <= 4; ++i) {
		bfmt_translate(OUT, &english, bfmt_ordinal(n, i, (one, "#st"), (two, "#nd"), (few, "#rd"), (other, "#th")), " ");
	}
	EXPECT_OUTPUT("1st 2nd 3rd 4th ");
}
