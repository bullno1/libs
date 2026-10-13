#ifndef BFMT_CLDR_H
#define BFMT_CLDR_H

// Generated from CLDR 48 (cldr-json 48.0.0) by `scripts/bfmt_cldr.py`
// out of `scripts/bfmt_cldr.h.in`.
// Do not edit; change the template or rerun the script for a newer CLDR.

/**
 * @file
 * The plural rules of every language, from CLDR.
 *
 * @code{.c}
 * bfmt_locale_t locale = { .plural_rule = bfmt_cldr_plural_rule("pl") };
 * @endcode
 *
 * In **exactly one** source file, define `BFMT_CLDR_IMPLEMENTATION` before
 * including bfmt_cldr.h.
 */

#include "bfmt.h"

/**
 * The plural rule of a language.
 *
 * @param language the language as specified by gettext or BCP 47: `en`,
 *   `pt_BR`, `pt-PT`, `sr@latin` or `de_DE.UTF-8`.
 *   The search is case-insensitive. A charset or modifier is ignored.
 *   A region or script CLDR does not distinguish falls back to the language
 *   alone.
 * @return the rules, or NULL for a language CLDR has none for
 */
BFMT_API bfmt_plural_rule_t*
bfmt_cldr_plural_rule(const char* language);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BFMT_CLDR_IMPLEMENTATION)
#define BFMT_CLDR_IMPLEMENTATION
#endif

#if defined(BFMT_CLDR_IMPLEMENTATION) && !defined(BFMT_CLDR_IMPLEMENTED)
#define BFMT_CLDR_IMPLEMENTED

#include <string.h>

// Rules {{{

// af, an, bg, ce, el, eu, gsw, ky, ml, mn, nb, no, ps, sd, ta, te, tr, uz
static bfmt_plural_t
bfmt_cldr__rule_af(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// ak, bho, csw, guw, ln, mg, nso, ti, wa
static bfmt_plural_t
bfmt_cldr__rule_ak(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 0..1
	if (num.f == 0 && (num.i <= 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// am, fa, kn, zu
static bfmt_plural_t
bfmt_cldr__rule_am(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// ar
static bfmt_plural_t
bfmt_cldr__rule_ar(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n % 100 = 3..10
	if (num.f == 0 && ((3 <= (num.i % 100) && (num.i % 100) <= 10))) {
		return BFMT_PLURAL_FEW;
	}
	// many: n % 100 = 11..99
	if (num.f == 0 && ((11 <= (num.i % 100) && (num.i % 100) <= 99))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ars
static bfmt_plural_t
bfmt_cldr__rule_ars(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n % 100 = 3..10
	if (num.f == 0 && ((3 <= (num.i % 100) && (num.i % 100) <= 10))) {
		return BFMT_PLURAL_FEW;
	}
	// many: n % 100 = 11..99
	if (num.f == 0 && ((11 <= (num.i % 100) && (num.i % 100) <= 99))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// as, bn
static bfmt_plural_t
bfmt_cldr__rule_as(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1,5,7,8,9,10
		if (num.f == 0 && (num.i == 1 || num.i == 5 || num.i == 7 || num.i == 8 || num.i == 9 || num.i == 10)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,3
		if (num.f == 0 && (num.i == 2 || num.i == 3)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		// many: n = 6
		if (num.f == 0 && (num.i == 6)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// asa, bem, bez, brx, cgg, chr, ckb, dv, ee, eo, fo, fur, ha, haw, jgo, jmc, kaj, kcg, kkj, kl, ks, ksb, ku, lb, lg, mas, mgo, nah, nd, nn, nnh, nr, ny, nyn, om, os, pap, rm, rof, rwk, saq, sdh, seh, sn, so, ss, ssy, st, syr, teo, tig, tn, ts, ug, ve, vo, vun, wae, xh, xog
static bfmt_plural_t
bfmt_cldr__rule_asa(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// ast, de, et, fi, fy, gl, ia, ie, nl, sw, ur
static bfmt_plural_t
bfmt_cldr__rule_ast(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// az
static bfmt_plural_t
bfmt_cldr__rule_az(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: i % 10 = 1,2,5,7,8 or i % 100 = 20,50,70,80
		if (
			((num.i % 10) == 1 || (num.i % 10) == 2 || (num.i % 10) == 5 || (num.i % 10) == 7 || (num.i % 10) == 8) ||
			((num.i % 100) == 20 || (num.i % 100) == 50 || (num.i % 100) == 70 || (num.i % 100) == 80)
		) {
			return BFMT_PLURAL_ONE;
		}
		// few: i % 10 = 3,4 or i % 1000 = 100,200,300,400,500,600,700,800,900
		if (
			((num.i % 10) == 3 || (num.i % 10) == 4) ||
			((num.i % 1000) == 100 || (num.i % 1000) == 200 || (num.i % 1000) == 300 || (num.i % 1000) == 400 || (num.i % 1000) == 500 || (num.i % 1000) == 600 || (num.i % 1000) == 700 || (num.i % 1000) == 800 || (num.i % 1000) == 900)
		) {
			return BFMT_PLURAL_FEW;
		}
		// many: i = 0 or i % 10 = 6 or i % 100 = 40,60,90
		if (
			(num.i == 0) ||
			((num.i % 10) == 6) ||
			((num.i % 100) == 40 || (num.i % 100) == 60 || (num.i % 100) == 90)
		) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// bal
static bfmt_plural_t
bfmt_cldr__rule_bal(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// be
static bfmt_plural_t
bfmt_cldr__rule_be(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// few: n % 10 = 2,3 and n % 100 != 12,13
		if ((num.f == 0 && ((num.i % 10) == 2 || (num.i % 10) == 3)) && (num.f != 0 || !((num.i % 100) == 12 || (num.i % 100) == 13))) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n % 10 = 1 and n % 100 != 11
	if ((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((num.i % 100) == 11))) {
		return BFMT_PLURAL_ONE;
	}
	// few: n % 10 = 2..4 and n % 100 != 12..14
	if ((num.f == 0 && ((2 <= (num.i % 10) && (num.i % 10) <= 4))) && (num.f != 0 || !((12 <= (num.i % 100) && (num.i % 100) <= 14)))) {
		return BFMT_PLURAL_FEW;
	}
	// many: n % 10 = 0 or n % 10 = 5..9 or n % 100 = 11..14
	if (
		(num.f == 0 && ((num.i % 10) == 0)) ||
		(num.f == 0 && ((5 <= (num.i % 10) && (num.i % 10) <= 9))) ||
		(num.f == 0 && ((11 <= (num.i % 100) && (num.i % 100) <= 14)))
	) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// blo
static bfmt_plural_t
bfmt_cldr__rule_blo(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// zero: i = 0
		if (num.i == 0) {
			return BFMT_PLURAL_ZERO;
		}
		// one: i = 1
		if (num.i == 1) {
			return BFMT_PLURAL_ONE;
		}
		// few: i = 2,3,4,5,6
		if (num.i == 2 || num.i == 3 || num.i == 4 || num.i == 5 || num.i == 6) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// bm, bo, dz, hnj, ig, ii, jbo, jv, jw, kde, kea, lkt, nqo, osa, sah, ses, sg, su, to, wo, yo
static bfmt_plural_t
bfmt_cldr__rule_bm(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	return BFMT_PLURAL_OTHER;
}

// br
static bfmt_plural_t
bfmt_cldr__rule_br(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n % 10 = 1 and n % 100 != 11,71,91
	if ((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((num.i % 100) == 11 || (num.i % 100) == 71 || (num.i % 100) == 91))) {
		return BFMT_PLURAL_ONE;
	}
	// two: n % 10 = 2 and n % 100 != 12,72,92
	if ((num.f == 0 && ((num.i % 10) == 2)) && (num.f != 0 || !((num.i % 100) == 12 || (num.i % 100) == 72 || (num.i % 100) == 92))) {
		return BFMT_PLURAL_TWO;
	}
	// few: n % 10 = 3..4,9 and n % 100 != 10..19,70..79,90..99
	if ((num.f == 0 && ((3 <= (num.i % 10) && (num.i % 10) <= 4) || (num.i % 10) == 9)) && (num.f != 0 || !((10 <= (num.i % 100) && (num.i % 100) <= 19) || (70 <= (num.i % 100) && (num.i % 100) <= 79) || (90 <= (num.i % 100) && (num.i % 100) <= 99)))) {
		return BFMT_PLURAL_FEW;
	}
	// many: n != 0 and n % 1000000 = 0
	if ((num.f != 0 || !(num.i == 0)) && (num.f == 0 && ((num.i % 1000000) == 0))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// bs, hr, sh, sr
static bfmt_plural_t
bfmt_cldr__rule_bs(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11
	if (
		((num.v == 0) && ((num.i % 10) == 1) && !((num.i % 100) == 11)) ||
		(((num.f % 10) == 1) && !((num.f % 100) == 11))
	) {
		return BFMT_PLURAL_ONE;
	}
	// few: v = 0 and i % 10 = 2..4 and i % 100 != 12..14 or f % 10 = 2..4 and f % 100 != 12..14
	if (
		((num.v == 0) && ((2 <= (num.i % 10) && (num.i % 10) <= 4)) && !((12 <= (num.i % 100) && (num.i % 100) <= 14))) ||
		(((2 <= (num.f % 10) && (num.f % 10) <= 4)) && !((12 <= (num.f % 100) && (num.f % 100) <= 14)))
	) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// ca
static bfmt_plural_t
bfmt_cldr__rule_ca(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1,3
		if (num.f == 0 && (num.i == 1 || num.i == 3)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2
		if (num.f == 0 && (num.i == 2)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ceb
static bfmt_plural_t
bfmt_cldr__rule_ceb(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i = 1,2,3 or v = 0 and i % 10 != 4,6,9 or v != 0 and f % 10 != 4,6,9
	if (
		((num.v == 0) && (num.i == 1 || num.i == 2 || num.i == 3)) ||
		((num.v == 0) && !((num.i % 10) == 4 || (num.i % 10) == 6 || (num.i % 10) == 9)) ||
		(!(num.v == 0) && !((num.f % 10) == 4 || (num.f % 10) == 6 || (num.f % 10) == 9))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// cs, sk
static bfmt_plural_t
bfmt_cldr__rule_cs(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// few: i = 2..4 and v = 0
	if (((2 <= num.i && num.i <= 4)) && (num.v == 0)) {
		return BFMT_PLURAL_FEW;
	}
	// many: v != 0
	if (!(num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// cv
static bfmt_plural_t
bfmt_cldr__rule_cv(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// cy
static bfmt_plural_t
bfmt_cldr__rule_cy(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// zero: n = 0,7,8,9
		if (num.f == 0 && (num.i == 0 || num.i == 7 || num.i == 8 || num.i == 9)) {
			return BFMT_PLURAL_ZERO;
		}
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2
		if (num.f == 0 && (num.i == 2)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 3,4
		if (num.f == 0 && (num.i == 3 || num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		// many: n = 5,6
		if (num.f == 0 && (num.i == 5 || num.i == 6)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n = 3
	if (num.f == 0 && (num.i == 3)) {
		return BFMT_PLURAL_FEW;
	}
	// many: n = 6
	if (num.f == 0 && (num.i == 6)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// da
static bfmt_plural_t
bfmt_cldr__rule_da(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1 or t != 0 and i = 0,1
	if (
		(num.f == 0 && (num.i == 1)) ||
		(!(num.t == 0) && (num.i == 0 || num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// doi, pcm
static bfmt_plural_t
bfmt_cldr__rule_doi(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// dsb, hsb
static bfmt_plural_t
bfmt_cldr__rule_dsb(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 100 = 1 or f % 100 = 1
	if (
		((num.v == 0) && ((num.i % 100) == 1)) ||
		((num.f % 100) == 1)
	) {
		return BFMT_PLURAL_ONE;
	}
	// two: v = 0 and i % 100 = 2 or f % 100 = 2
	if (
		((num.v == 0) && ((num.i % 100) == 2)) ||
		((num.f % 100) == 2)
	) {
		return BFMT_PLURAL_TWO;
	}
	// few: v = 0 and i % 100 = 3..4 or f % 100 = 3..4
	if (
		((num.v == 0) && ((3 <= (num.i % 100) && (num.i % 100) <= 4))) ||
		((3 <= (num.f % 100) && (num.f % 100) <= 4))
	) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// en
static bfmt_plural_t
bfmt_cldr__rule_en(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n % 10 = 1 and n % 100 != 11
		if ((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((num.i % 100) == 11))) {
			return BFMT_PLURAL_ONE;
		}
		// two: n % 10 = 2 and n % 100 != 12
		if ((num.f == 0 && ((num.i % 10) == 2)) && (num.f != 0 || !((num.i % 100) == 12))) {
			return BFMT_PLURAL_TWO;
		}
		// few: n % 10 = 3 and n % 100 != 13
		if ((num.f == 0 && ((num.i % 10) == 3)) && (num.f != 0 || !((num.i % 100) == 13))) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// es
static bfmt_plural_t
bfmt_cldr__rule_es(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ff, kab
static bfmt_plural_t
bfmt_cldr__rule_ff(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0,1
	if (num.i == 0 || num.i == 1) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// fil, tl
static bfmt_plural_t
bfmt_cldr__rule_fil(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i = 1,2,3 or v = 0 and i % 10 != 4,6,9 or v != 0 and f % 10 != 4,6,9
	if (
		((num.v == 0) && (num.i == 1 || num.i == 2 || num.i == 3)) ||
		((num.v == 0) && !((num.i % 10) == 4 || (num.i % 10) == 6 || (num.i % 10) == 9)) ||
		(!(num.v == 0) && !((num.f % 10) == 4 || (num.f % 10) == 6 || (num.f % 10) == 9))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// fr
static bfmt_plural_t
bfmt_cldr__rule_fr(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0,1
	if (num.i == 0 || num.i == 1) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ga
static bfmt_plural_t
bfmt_cldr__rule_ga(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n = 3..6
	if (num.f == 0 && ((3 <= num.i && num.i <= 6))) {
		return BFMT_PLURAL_FEW;
	}
	// many: n = 7..10
	if (num.f == 0 && ((7 <= num.i && num.i <= 10))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// gd
static bfmt_plural_t
bfmt_cldr__rule_gd(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1,11
		if (num.f == 0 && (num.i == 1 || num.i == 11)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,12
		if (num.f == 0 && (num.i == 2 || num.i == 12)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 3,13
		if (num.f == 0 && (num.i == 3 || num.i == 13)) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1,11
	if (num.f == 0 && (num.i == 1 || num.i == 11)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2,12
	if (num.f == 0 && (num.i == 2 || num.i == 12)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n = 3..10,13..19
	if (num.f == 0 && ((3 <= num.i && num.i <= 10) || (13 <= num.i && num.i <= 19))) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// gu, hi
static bfmt_plural_t
bfmt_cldr__rule_gu(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,3
		if (num.f == 0 && (num.i == 2 || num.i == 3)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		// many: n = 6
		if (num.f == 0 && (num.i == 6)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// gv
static bfmt_plural_t
bfmt_cldr__rule_gv(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 10 = 1
	if ((num.v == 0) && ((num.i % 10) == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: v = 0 and i % 10 = 2
	if ((num.v == 0) && ((num.i % 10) == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: v = 0 and i % 100 = 0,20,40,60,80
	if ((num.v == 0) && ((num.i % 100) == 0 || (num.i % 100) == 20 || (num.i % 100) == 40 || (num.i % 100) == 60 || (num.i % 100) == 80)) {
		return BFMT_PLURAL_FEW;
	}
	// many: v != 0
	if (!(num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// he
static bfmt_plural_t
bfmt_cldr__rule_he(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0 or i = 0 and v != 0
	if (
		((num.i == 1) && (num.v == 0)) ||
		((num.i == 0) && !(num.v == 0))
	) {
		return BFMT_PLURAL_ONE;
	}
	// two: i = 2 and v = 0
	if ((num.i == 2) && (num.v == 0)) {
		return BFMT_PLURAL_TWO;
	}
	return BFMT_PLURAL_OTHER;
}

// hu
static bfmt_plural_t
bfmt_cldr__rule_hu(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1,5
		if (num.f == 0 && (num.i == 1 || num.i == 5)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// hy
static bfmt_plural_t
bfmt_cldr__rule_hy(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0,1
	if (num.i == 0 || num.i == 1) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// id, ja, km, ko, my, th, tpi, und, yue, zh
static bfmt_plural_t
bfmt_cldr__rule_id(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	return BFMT_PLURAL_OTHER;
}

// io, yi
static bfmt_plural_t
bfmt_cldr__rule_io(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// is
static bfmt_plural_t
bfmt_cldr__rule_is(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: t = 0 and i % 10 = 1 and i % 100 != 11 or t % 10 = 1 and t % 100 != 11
	if (
		((num.t == 0) && ((num.i % 10) == 1) && !((num.i % 100) == 11)) ||
		(((num.t % 10) == 1) && !((num.t % 100) == 11))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// it, lld, vec
static bfmt_plural_t
bfmt_cldr__rule_it(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// many: n = 11,8,80,800
		if (num.f == 0 && (num.i == 11 || num.i == 8 || num.i == 80 || num.i == 800)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// iu, naq, sat, se, sma, smi, smj, smn, sms
static bfmt_plural_t
bfmt_cldr__rule_iu(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	return BFMT_PLURAL_OTHER;
}

// ka
static bfmt_plural_t
bfmt_cldr__rule_ka(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: i = 1
		if (num.i == 1) {
			return BFMT_PLURAL_ONE;
		}
		// many: i = 0 or i % 100 = 2..20,40,60,80
		if (
			(num.i == 0) ||
			((2 <= (num.i % 100) && (num.i % 100) <= 20) || (num.i % 100) == 40 || (num.i % 100) == 60 || (num.i % 100) == 80)
		) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// kk
static bfmt_plural_t
bfmt_cldr__rule_kk(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// many: n % 10 = 6 or n % 10 = 9 or n % 10 = 0 and n != 0
		if (
			(num.f == 0 && ((num.i % 10) == 6)) ||
			(num.f == 0 && ((num.i % 10) == 9)) ||
			((num.f == 0 && ((num.i % 10) == 0)) && (num.f != 0 || !(num.i == 0)))
		) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// kok, kok-latn
static bfmt_plural_t
bfmt_cldr__rule_kok(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,3
		if (num.f == 0 && (num.i == 2 || num.i == 3)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// ksh
static bfmt_plural_t
bfmt_cldr__rule_ksh(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// kw
static bfmt_plural_t
bfmt_cldr__rule_kw(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1..4 or n % 100 = 1..4,21..24,41..44,61..64,81..84
		if (
			(num.f == 0 && ((1 <= num.i && num.i <= 4))) ||
			(num.f == 0 && ((1 <= (num.i % 100) && (num.i % 100) <= 4) || (21 <= (num.i % 100) && (num.i % 100) <= 24) || (41 <= (num.i % 100) && (num.i % 100) <= 44) || (61 <= (num.i % 100) && (num.i % 100) <= 64) || (81 <= (num.i % 100) && (num.i % 100) <= 84)))
		) {
			return BFMT_PLURAL_ONE;
		}
		// many: n = 5 or n % 100 = 5
		if (
			(num.f == 0 && (num.i == 5)) ||
			(num.f == 0 && ((num.i % 100) == 5))
		) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n % 100 = 2,22,42,62,82 or n % 1000 = 0 and n % 100000 = 1000..20000,40000,60000,80000 or n != 0 and n % 1000000 = 100000
	if (
		(num.f == 0 && ((num.i % 100) == 2 || (num.i % 100) == 22 || (num.i % 100) == 42 || (num.i % 100) == 62 || (num.i % 100) == 82)) ||
		((num.f == 0 && ((num.i % 1000) == 0)) && (num.f == 0 && ((1000 <= (num.i % 100000) && (num.i % 100000) <= 20000) || (num.i % 100000) == 40000 || (num.i % 100000) == 60000 || (num.i % 100000) == 80000))) ||
		((num.f != 0 || !(num.i == 0)) && (num.f == 0 && ((num.i % 1000000) == 100000)))
	) {
		return BFMT_PLURAL_TWO;
	}
	// few: n % 100 = 3,23,43,63,83
	if (num.f == 0 && ((num.i % 100) == 3 || (num.i % 100) == 23 || (num.i % 100) == 43 || (num.i % 100) == 63 || (num.i % 100) == 83)) {
		return BFMT_PLURAL_FEW;
	}
	// many: n != 1 and n % 100 = 1,21,41,61,81
	if ((num.f != 0 || !(num.i == 1)) && (num.f == 0 && ((num.i % 100) == 1 || (num.i % 100) == 21 || (num.i % 100) == 41 || (num.i % 100) == 61 || (num.i % 100) == 81))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// lag
static bfmt_plural_t
bfmt_cldr__rule_lag(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n = 0
	if (num.f == 0 && (num.i == 0)) {
		return BFMT_PLURAL_ZERO;
	}
	// one: i = 0,1 and n != 0
	if ((num.i == 0 || num.i == 1) && (num.f != 0 || !(num.i == 0))) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// lij
static bfmt_plural_t
bfmt_cldr__rule_lij(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// many: n = 11,8,80..89,800..899
		if (num.f == 0 && (num.i == 11 || num.i == 8 || (80 <= num.i && num.i <= 89) || (800 <= num.i && num.i <= 899))) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// lo, ms, vi
static bfmt_plural_t
bfmt_cldr__rule_lo(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	return BFMT_PLURAL_OTHER;
}

// lt
static bfmt_plural_t
bfmt_cldr__rule_lt(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n % 10 = 1 and n % 100 != 11..19
	if ((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((11 <= (num.i % 100) && (num.i % 100) <= 19)))) {
		return BFMT_PLURAL_ONE;
	}
	// few: n % 10 = 2..9 and n % 100 != 11..19
	if ((num.f == 0 && ((2 <= (num.i % 10) && (num.i % 10) <= 9))) && (num.f != 0 || !((11 <= (num.i % 100) && (num.i % 100) <= 19)))) {
		return BFMT_PLURAL_FEW;
	}
	// many: f != 0
	if (!(num.f == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// lv, prg
static bfmt_plural_t
bfmt_cldr__rule_lv(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// zero: n % 10 = 0 or n % 100 = 11..19 or v = 2 and f % 100 = 11..19
	if (
		(num.f == 0 && ((num.i % 10) == 0)) ||
		(num.f == 0 && ((11 <= (num.i % 100) && (num.i % 100) <= 19))) ||
		((num.v == 2) && ((11 <= (num.f % 100) && (num.f % 100) <= 19)))
	) {
		return BFMT_PLURAL_ZERO;
	}
	// one: n % 10 = 1 and n % 100 != 11 or v = 2 and f % 10 = 1 and f % 100 != 11 or v != 2 and f % 10 = 1
	if (
		((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((num.i % 100) == 11))) ||
		((num.v == 2) && ((num.f % 10) == 1) && !((num.f % 100) == 11)) ||
		(!(num.v == 2) && ((num.f % 10) == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// mk
static bfmt_plural_t
bfmt_cldr__rule_mk(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: i % 10 = 1 and i % 100 != 11
		if (((num.i % 10) == 1) && !((num.i % 100) == 11)) {
			return BFMT_PLURAL_ONE;
		}
		// two: i % 10 = 2 and i % 100 != 12
		if (((num.i % 10) == 2) && !((num.i % 100) == 12)) {
			return BFMT_PLURAL_TWO;
		}
		// many: i % 10 = 7,8 and i % 100 != 17,18
		if (((num.i % 10) == 7 || (num.i % 10) == 8) && !((num.i % 100) == 17 || (num.i % 100) == 18)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11
	if (
		((num.v == 0) && ((num.i % 10) == 1) && !((num.i % 100) == 11)) ||
		(((num.f % 10) == 1) && !((num.f % 100) == 11))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// mo, ro
static bfmt_plural_t
bfmt_cldr__rule_mo(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// few: v != 0 or n = 0 or n != 1 and n % 100 = 1..19
	if (
		!(num.v == 0) ||
		(num.f == 0 && (num.i == 0)) ||
		((num.f != 0 || !(num.i == 1)) && (num.f == 0 && ((1 <= (num.i % 100) && (num.i % 100) <= 19))))
	) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// mr
static bfmt_plural_t
bfmt_cldr__rule_mr(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,3
		if (num.f == 0 && (num.i == 2 || num.i == 3)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// mt
static bfmt_plural_t
bfmt_cldr__rule_mt(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n = 0 or n % 100 = 3..10
	if (
		(num.f == 0 && (num.i == 0)) ||
		(num.f == 0 && ((3 <= (num.i % 100) && (num.i % 100) <= 10)))
	) {
		return BFMT_PLURAL_FEW;
	}
	// many: n % 100 = 11..19
	if (num.f == 0 && ((11 <= (num.i % 100) && (num.i % 100) <= 19))) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ne
static bfmt_plural_t
bfmt_cldr__rule_ne(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1..4
		if (num.f == 0 && ((1 <= num.i && num.i <= 4))) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// or
static bfmt_plural_t
bfmt_cldr__rule_or(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1,5,7..9
		if (num.f == 0 && (num.i == 1 || num.i == 5 || (7 <= num.i && num.i <= 9))) {
			return BFMT_PLURAL_ONE;
		}
		// two: n = 2,3
		if (num.f == 0 && (num.i == 2 || num.i == 3)) {
			return BFMT_PLURAL_TWO;
		}
		// few: n = 4
		if (num.f == 0 && (num.i == 4)) {
			return BFMT_PLURAL_FEW;
		}
		// many: n = 6
		if (num.f == 0 && (num.i == 6)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// pa
static bfmt_plural_t
bfmt_cldr__rule_pa(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 0..1
	if (num.f == 0 && (num.i <= 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// pl
static bfmt_plural_t
bfmt_cldr__rule_pl(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// few: v = 0 and i % 10 = 2..4 and i % 100 != 12..14
	if ((num.v == 0) && ((2 <= (num.i % 10) && (num.i % 10) <= 4)) && !((12 <= (num.i % 100) && (num.i % 100) <= 14))) {
		return BFMT_PLURAL_FEW;
	}
	// many: v = 0 and i != 1 and i % 10 = 0..1 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 12..14
	if (
		((num.v == 0) && !(num.i == 1) && ((num.i % 10) <= 1)) ||
		((num.v == 0) && ((5 <= (num.i % 10) && (num.i % 10) <= 9))) ||
		((num.v == 0) && ((12 <= (num.i % 100) && (num.i % 100) <= 14)))
	) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// pt
static bfmt_plural_t
bfmt_cldr__rule_pt(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0..1
	if (num.i <= 1) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// pt-pt
static bfmt_plural_t
bfmt_cldr__rule_pt_pt(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// ru
static bfmt_plural_t
bfmt_cldr__rule_ru(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 10 = 1 and i % 100 != 11
	if ((num.v == 0) && ((num.i % 10) == 1) && !((num.i % 100) == 11)) {
		return BFMT_PLURAL_ONE;
	}
	// few: v = 0 and i % 10 = 2..4 and i % 100 != 12..14
	if ((num.v == 0) && ((2 <= (num.i % 10) && (num.i % 10) <= 4)) && !((12 <= (num.i % 100) && (num.i % 100) <= 14))) {
		return BFMT_PLURAL_FEW;
	}
	// many: v = 0 and i % 10 = 0 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 11..14
	if (
		((num.v == 0) && ((num.i % 10) == 0)) ||
		((num.v == 0) && ((5 <= (num.i % 10) && (num.i % 10) <= 9))) ||
		((num.v == 0) && ((11 <= (num.i % 100) && (num.i % 100) <= 14)))
	) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// sc
static bfmt_plural_t
bfmt_cldr__rule_sc(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// many: n = 11,8,80,800
		if (num.f == 0 && (num.i == 11 || num.i == 8 || num.i == 80 || num.i == 800)) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// scn
static bfmt_plural_t
bfmt_cldr__rule_scn(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// many: n = 11,8,80..89,800..899
		if (num.f == 0 && (num.i == 11 || num.i == 8 || (80 <= num.i && num.i <= 89) || (800 <= num.i && num.i <= 899))) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	// many: e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5
	if (!(num.i == 0) && ((num.i % 1000000) == 0) && (num.v == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// sgs
static bfmt_plural_t
bfmt_cldr__rule_sgs(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n % 10 = 1 and n % 100 != 11
	if ((num.f == 0 && ((num.i % 10) == 1)) && (num.f != 0 || !((num.i % 100) == 11))) {
		return BFMT_PLURAL_ONE;
	}
	// two: n = 2
	if (num.f == 0 && (num.i == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: n != 2 and n % 10 = 2..9 and n % 100 != 11..19
	if ((num.f != 0 || !(num.i == 2)) && (num.f == 0 && ((2 <= (num.i % 10) && (num.i % 10) <= 9))) && (num.f != 0 || !((11 <= (num.i % 100) && (num.i % 100) <= 19)))) {
		return BFMT_PLURAL_FEW;
	}
	// many: f != 0
	if (!(num.f == 0)) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// shi
static bfmt_plural_t
bfmt_cldr__rule_shi(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 0 or n = 1
	if (
		(num.i == 0) ||
		(num.f == 0 && (num.i == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	// few: n = 2..10
	if (num.f == 0 && ((2 <= num.i && num.i <= 10))) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// si
static bfmt_plural_t
bfmt_cldr__rule_si(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 0,1 or i = 0 and f = 1
	if (
		(num.f == 0 && (num.i == 0 || num.i == 1)) ||
		((num.i == 0) && (num.f == 1))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// sl
static bfmt_plural_t
bfmt_cldr__rule_sl(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 100 = 1
	if ((num.v == 0) && ((num.i % 100) == 1)) {
		return BFMT_PLURAL_ONE;
	}
	// two: v = 0 and i % 100 = 2
	if ((num.v == 0) && ((num.i % 100) == 2)) {
		return BFMT_PLURAL_TWO;
	}
	// few: v = 0 and i % 100 = 3..4 or v != 0
	if (
		((num.v == 0) && ((3 <= (num.i % 100) && (num.i % 100) <= 4))) ||
		!(num.v == 0)
	) {
		return BFMT_PLURAL_FEW;
	}
	return BFMT_PLURAL_OTHER;
}

// sq
static bfmt_plural_t
bfmt_cldr__rule_sq(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n = 1
		if (num.f == 0 && (num.i == 1)) {
			return BFMT_PLURAL_ONE;
		}
		// many: n % 10 = 4 and n % 100 != 14
		if ((num.f == 0 && ((num.i % 10) == 4)) && (num.f != 0 || !((num.i % 100) == 14))) {
			return BFMT_PLURAL_MANY;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// sv
static bfmt_plural_t
bfmt_cldr__rule_sv(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// one: n % 10 = 1,2 and n % 100 != 11,12
		if ((num.f == 0 && ((num.i % 10) == 1 || (num.i % 10) == 2)) && (num.f != 0 || !((num.i % 100) == 11 || (num.i % 100) == 12))) {
			return BFMT_PLURAL_ONE;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: i = 1 and v = 0
	if ((num.i == 1) && (num.v == 0)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// tk
static bfmt_plural_t
bfmt_cldr__rule_tk(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// few: n % 10 = 6,9 or n = 10
		if (
			(num.f == 0 && ((num.i % 10) == 6 || (num.i % 10) == 9)) ||
			(num.f == 0 && (num.i == 10))
		) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 1
	if (num.f == 0 && (num.i == 1)) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// tzm
static bfmt_plural_t
bfmt_cldr__rule_tzm(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		return BFMT_PLURAL_OTHER;
	}

	// one: n = 0..1 or n = 11..99
	if (
		(num.f == 0 && (num.i <= 1)) ||
		(num.f == 0 && ((11 <= num.i && num.i <= 99)))
	) {
		return BFMT_PLURAL_ONE;
	}
	return BFMT_PLURAL_OTHER;
}

// uk
static bfmt_plural_t
bfmt_cldr__rule_uk(bfmt_plural_num_t num, bfmt_plural_type_t type) {
	(void)num;
	if (type == BFMT_PLURAL_ORDINAL) {
		// few: n % 10 = 3 and n % 100 != 13
		if ((num.f == 0 && ((num.i % 10) == 3)) && (num.f != 0 || !((num.i % 100) == 13))) {
			return BFMT_PLURAL_FEW;
		}
		return BFMT_PLURAL_OTHER;
	}

	// one: v = 0 and i % 10 = 1 and i % 100 != 11
	if ((num.v == 0) && ((num.i % 10) == 1) && !((num.i % 100) == 11)) {
		return BFMT_PLURAL_ONE;
	}
	// few: v = 0 and i % 10 = 2..4 and i % 100 != 12..14
	if ((num.v == 0) && ((2 <= (num.i % 10) && (num.i % 10) <= 4)) && !((12 <= (num.i % 100) && (num.i % 100) <= 14))) {
		return BFMT_PLURAL_FEW;
	}
	// many: v = 0 and i % 10 = 0 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 11..14
	if (
		((num.v == 0) && ((num.i % 10) == 0)) ||
		((num.v == 0) && ((5 <= (num.i % 10) && (num.i % 10) <= 9))) ||
		((num.v == 0) && ((11 <= (num.i % 100) && (num.i % 100) <= 14)))
	) {
		return BFMT_PLURAL_MANY;
	}
	return BFMT_PLURAL_OTHER;
}

// }}}

// Lookup {{{

typedef struct {
	const char* tag;
	bfmt_plural_rule_t* rule;
} bfmt_cldr__entry_t;

// Sorted by tag, lowercase
static const bfmt_cldr__entry_t bfmt_cldr__table[] = {
	{ "af", bfmt_cldr__rule_af },
	{ "ak", bfmt_cldr__rule_ak },
	{ "am", bfmt_cldr__rule_am },
	{ "an", bfmt_cldr__rule_af },
	{ "ar", bfmt_cldr__rule_ar },
	{ "ars", bfmt_cldr__rule_ars },
	{ "as", bfmt_cldr__rule_as },
	{ "asa", bfmt_cldr__rule_asa },
	{ "ast", bfmt_cldr__rule_ast },
	{ "az", bfmt_cldr__rule_az },
	{ "bal", bfmt_cldr__rule_bal },
	{ "be", bfmt_cldr__rule_be },
	{ "bem", bfmt_cldr__rule_asa },
	{ "bez", bfmt_cldr__rule_asa },
	{ "bg", bfmt_cldr__rule_af },
	{ "bho", bfmt_cldr__rule_ak },
	{ "blo", bfmt_cldr__rule_blo },
	{ "bm", bfmt_cldr__rule_bm },
	{ "bn", bfmt_cldr__rule_as },
	{ "bo", bfmt_cldr__rule_bm },
	{ "br", bfmt_cldr__rule_br },
	{ "brx", bfmt_cldr__rule_asa },
	{ "bs", bfmt_cldr__rule_bs },
	{ "ca", bfmt_cldr__rule_ca },
	{ "ce", bfmt_cldr__rule_af },
	{ "ceb", bfmt_cldr__rule_ceb },
	{ "cgg", bfmt_cldr__rule_asa },
	{ "chr", bfmt_cldr__rule_asa },
	{ "ckb", bfmt_cldr__rule_asa },
	{ "cs", bfmt_cldr__rule_cs },
	{ "csw", bfmt_cldr__rule_ak },
	{ "cv", bfmt_cldr__rule_cv },
	{ "cy", bfmt_cldr__rule_cy },
	{ "da", bfmt_cldr__rule_da },
	{ "de", bfmt_cldr__rule_ast },
	{ "doi", bfmt_cldr__rule_doi },
	{ "dsb", bfmt_cldr__rule_dsb },
	{ "dv", bfmt_cldr__rule_asa },
	{ "dz", bfmt_cldr__rule_bm },
	{ "ee", bfmt_cldr__rule_asa },
	{ "el", bfmt_cldr__rule_af },
	{ "en", bfmt_cldr__rule_en },
	{ "eo", bfmt_cldr__rule_asa },
	{ "es", bfmt_cldr__rule_es },
	{ "et", bfmt_cldr__rule_ast },
	{ "eu", bfmt_cldr__rule_af },
	{ "fa", bfmt_cldr__rule_am },
	{ "ff", bfmt_cldr__rule_ff },
	{ "fi", bfmt_cldr__rule_ast },
	{ "fil", bfmt_cldr__rule_fil },
	{ "fo", bfmt_cldr__rule_asa },
	{ "fr", bfmt_cldr__rule_fr },
	{ "fur", bfmt_cldr__rule_asa },
	{ "fy", bfmt_cldr__rule_ast },
	{ "ga", bfmt_cldr__rule_ga },
	{ "gd", bfmt_cldr__rule_gd },
	{ "gl", bfmt_cldr__rule_ast },
	{ "gsw", bfmt_cldr__rule_af },
	{ "gu", bfmt_cldr__rule_gu },
	{ "guw", bfmt_cldr__rule_ak },
	{ "gv", bfmt_cldr__rule_gv },
	{ "ha", bfmt_cldr__rule_asa },
	{ "haw", bfmt_cldr__rule_asa },
	{ "he", bfmt_cldr__rule_he },
	{ "hi", bfmt_cldr__rule_gu },
	{ "hnj", bfmt_cldr__rule_bm },
	{ "hr", bfmt_cldr__rule_bs },
	{ "hsb", bfmt_cldr__rule_dsb },
	{ "hu", bfmt_cldr__rule_hu },
	{ "hy", bfmt_cldr__rule_hy },
	{ "ia", bfmt_cldr__rule_ast },
	{ "id", bfmt_cldr__rule_id },
	{ "ie", bfmt_cldr__rule_ast },
	{ "ig", bfmt_cldr__rule_bm },
	{ "ii", bfmt_cldr__rule_bm },
	{ "io", bfmt_cldr__rule_io },
	{ "is", bfmt_cldr__rule_is },
	{ "it", bfmt_cldr__rule_it },
	{ "iu", bfmt_cldr__rule_iu },
	{ "ja", bfmt_cldr__rule_id },
	{ "jbo", bfmt_cldr__rule_bm },
	{ "jgo", bfmt_cldr__rule_asa },
	{ "jmc", bfmt_cldr__rule_asa },
	{ "jv", bfmt_cldr__rule_bm },
	{ "jw", bfmt_cldr__rule_bm },
	{ "ka", bfmt_cldr__rule_ka },
	{ "kab", bfmt_cldr__rule_ff },
	{ "kaj", bfmt_cldr__rule_asa },
	{ "kcg", bfmt_cldr__rule_asa },
	{ "kde", bfmt_cldr__rule_bm },
	{ "kea", bfmt_cldr__rule_bm },
	{ "kk", bfmt_cldr__rule_kk },
	{ "kkj", bfmt_cldr__rule_asa },
	{ "kl", bfmt_cldr__rule_asa },
	{ "km", bfmt_cldr__rule_id },
	{ "kn", bfmt_cldr__rule_am },
	{ "ko", bfmt_cldr__rule_id },
	{ "kok", bfmt_cldr__rule_kok },
	{ "kok-latn", bfmt_cldr__rule_kok },
	{ "ks", bfmt_cldr__rule_asa },
	{ "ksb", bfmt_cldr__rule_asa },
	{ "ksh", bfmt_cldr__rule_ksh },
	{ "ku", bfmt_cldr__rule_asa },
	{ "kw", bfmt_cldr__rule_kw },
	{ "ky", bfmt_cldr__rule_af },
	{ "lag", bfmt_cldr__rule_lag },
	{ "lb", bfmt_cldr__rule_asa },
	{ "lg", bfmt_cldr__rule_asa },
	{ "lij", bfmt_cldr__rule_lij },
	{ "lkt", bfmt_cldr__rule_bm },
	{ "lld", bfmt_cldr__rule_it },
	{ "ln", bfmt_cldr__rule_ak },
	{ "lo", bfmt_cldr__rule_lo },
	{ "lt", bfmt_cldr__rule_lt },
	{ "lv", bfmt_cldr__rule_lv },
	{ "mas", bfmt_cldr__rule_asa },
	{ "mg", bfmt_cldr__rule_ak },
	{ "mgo", bfmt_cldr__rule_asa },
	{ "mk", bfmt_cldr__rule_mk },
	{ "ml", bfmt_cldr__rule_af },
	{ "mn", bfmt_cldr__rule_af },
	{ "mo", bfmt_cldr__rule_mo },
	{ "mr", bfmt_cldr__rule_mr },
	{ "ms", bfmt_cldr__rule_lo },
	{ "mt", bfmt_cldr__rule_mt },
	{ "my", bfmt_cldr__rule_id },
	{ "nah", bfmt_cldr__rule_asa },
	{ "naq", bfmt_cldr__rule_iu },
	{ "nb", bfmt_cldr__rule_af },
	{ "nd", bfmt_cldr__rule_asa },
	{ "ne", bfmt_cldr__rule_ne },
	{ "nl", bfmt_cldr__rule_ast },
	{ "nn", bfmt_cldr__rule_asa },
	{ "nnh", bfmt_cldr__rule_asa },
	{ "no", bfmt_cldr__rule_af },
	{ "nqo", bfmt_cldr__rule_bm },
	{ "nr", bfmt_cldr__rule_asa },
	{ "nso", bfmt_cldr__rule_ak },
	{ "ny", bfmt_cldr__rule_asa },
	{ "nyn", bfmt_cldr__rule_asa },
	{ "om", bfmt_cldr__rule_asa },
	{ "or", bfmt_cldr__rule_or },
	{ "os", bfmt_cldr__rule_asa },
	{ "osa", bfmt_cldr__rule_bm },
	{ "pa", bfmt_cldr__rule_pa },
	{ "pap", bfmt_cldr__rule_asa },
	{ "pcm", bfmt_cldr__rule_doi },
	{ "pl", bfmt_cldr__rule_pl },
	{ "prg", bfmt_cldr__rule_lv },
	{ "ps", bfmt_cldr__rule_af },
	{ "pt", bfmt_cldr__rule_pt },
	{ "pt-pt", bfmt_cldr__rule_pt_pt },
	{ "rm", bfmt_cldr__rule_asa },
	{ "ro", bfmt_cldr__rule_mo },
	{ "rof", bfmt_cldr__rule_asa },
	{ "ru", bfmt_cldr__rule_ru },
	{ "rwk", bfmt_cldr__rule_asa },
	{ "sah", bfmt_cldr__rule_bm },
	{ "saq", bfmt_cldr__rule_asa },
	{ "sat", bfmt_cldr__rule_iu },
	{ "sc", bfmt_cldr__rule_sc },
	{ "scn", bfmt_cldr__rule_scn },
	{ "sd", bfmt_cldr__rule_af },
	{ "sdh", bfmt_cldr__rule_asa },
	{ "se", bfmt_cldr__rule_iu },
	{ "seh", bfmt_cldr__rule_asa },
	{ "ses", bfmt_cldr__rule_bm },
	{ "sg", bfmt_cldr__rule_bm },
	{ "sgs", bfmt_cldr__rule_sgs },
	{ "sh", bfmt_cldr__rule_bs },
	{ "shi", bfmt_cldr__rule_shi },
	{ "si", bfmt_cldr__rule_si },
	{ "sk", bfmt_cldr__rule_cs },
	{ "sl", bfmt_cldr__rule_sl },
	{ "sma", bfmt_cldr__rule_iu },
	{ "smi", bfmt_cldr__rule_iu },
	{ "smj", bfmt_cldr__rule_iu },
	{ "smn", bfmt_cldr__rule_iu },
	{ "sms", bfmt_cldr__rule_iu },
	{ "sn", bfmt_cldr__rule_asa },
	{ "so", bfmt_cldr__rule_asa },
	{ "sq", bfmt_cldr__rule_sq },
	{ "sr", bfmt_cldr__rule_bs },
	{ "ss", bfmt_cldr__rule_asa },
	{ "ssy", bfmt_cldr__rule_asa },
	{ "st", bfmt_cldr__rule_asa },
	{ "su", bfmt_cldr__rule_bm },
	{ "sv", bfmt_cldr__rule_sv },
	{ "sw", bfmt_cldr__rule_ast },
	{ "syr", bfmt_cldr__rule_asa },
	{ "ta", bfmt_cldr__rule_af },
	{ "te", bfmt_cldr__rule_af },
	{ "teo", bfmt_cldr__rule_asa },
	{ "th", bfmt_cldr__rule_id },
	{ "ti", bfmt_cldr__rule_ak },
	{ "tig", bfmt_cldr__rule_asa },
	{ "tk", bfmt_cldr__rule_tk },
	{ "tl", bfmt_cldr__rule_fil },
	{ "tn", bfmt_cldr__rule_asa },
	{ "to", bfmt_cldr__rule_bm },
	{ "tpi", bfmt_cldr__rule_id },
	{ "tr", bfmt_cldr__rule_af },
	{ "ts", bfmt_cldr__rule_asa },
	{ "tzm", bfmt_cldr__rule_tzm },
	{ "ug", bfmt_cldr__rule_asa },
	{ "uk", bfmt_cldr__rule_uk },
	{ "und", bfmt_cldr__rule_id },
	{ "ur", bfmt_cldr__rule_ast },
	{ "uz", bfmt_cldr__rule_af },
	{ "ve", bfmt_cldr__rule_asa },
	{ "vec", bfmt_cldr__rule_it },
	{ "vi", bfmt_cldr__rule_lo },
	{ "vo", bfmt_cldr__rule_asa },
	{ "vun", bfmt_cldr__rule_asa },
	{ "wa", bfmt_cldr__rule_ak },
	{ "wae", bfmt_cldr__rule_asa },
	{ "wo", bfmt_cldr__rule_bm },
	{ "xh", bfmt_cldr__rule_asa },
	{ "xog", bfmt_cldr__rule_asa },
	{ "yi", bfmt_cldr__rule_io },
	{ "yo", bfmt_cldr__rule_bm },
	{ "yue", bfmt_cldr__rule_id },
	{ "zh", bfmt_cldr__rule_id },
	{ "zu", bfmt_cldr__rule_am },
};

static bfmt_plural_rule_t*
bfmt_cldr__find(const char* tag) {
	int lo = 0;
	int hi = (int)(sizeof(bfmt_cldr__table) / sizeof(bfmt_cldr__table[0])) - 1;
	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		int cmp = strcmp(tag, bfmt_cldr__table[mid].tag);
		if (cmp == 0) { return bfmt_cldr__table[mid].rule; }
		if (cmp < 0) { hi = mid - 1; } else { lo = mid + 1; }
	}
	return NULL;
}

bfmt_plural_rule_t*
bfmt_cldr_plural_rule(const char* language) {
	// Lowercase, `_` as `-`, without a charset or modifier
	char tag[32];
	int len = 0;
	for (const char* itr = language; *itr != '\0' && *itr != '.' && *itr != '@'; ++itr) {
		if (len >= (int)sizeof(tag) - 1) { break; }
		char c = *itr;
		if (c == '_') { c = '-'; }
		else if (c >= 'A' && c <= 'Z') { c = (char)(c - 'A' + 'a'); }
		tag[len++] = c;
	}

	// The whole tag first, then without its last subtag, down to the language
	while (len > 0) {
		tag[len] = '\0';
		bfmt_plural_rule_t* rule = bfmt_cldr__find(tag);
		if (rule != NULL) { return rule; }
		while (len > 0 && tag[len - 1] != '-') { --len; }
		if (len > 0) { --len; }
	}
	return NULL;
}

// }}}

#endif
