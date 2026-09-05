#include "../../barena.h"
#include "../../bhamt.h"
#include "../../btest.h"

#include <stdlib.h>
#include <string.h>

typedef BHAMT_TABLE(int, int) table_t;
typedef BHAMT_SET(int) set_t;

typedef struct {
	const char* chars;
	size_t len;
} str_t;

typedef BHAMT_TABLE(str_t, int) str_table_t;

// All bhamt allocations go through this arena.
// It is reset after every test so nothing is freed individually.
static barena_pool_t pool;
static barena_t arena;

static void
init_per_suite(void) {
	barena_pool_init(&pool, 64 * 1024);
}

static void
cleanup_per_suite(void) {
	barena_pool_cleanup(&pool);
}

static void
init_per_test(void) {
	barena_init(&arena, &pool);
}

static void
cleanup_per_test(void) {
	barena_reset(&arena);
}

static btest_suite_t hamt = {
	.name = "hamt",
	.init_per_suite = init_per_suite,
	.cleanup_per_suite = cleanup_per_suite,
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static bhamt_hash_t
str_hash(const void* key, size_t size) {
	(void)size;
	const str_t* str = key;
	return bhamt_hash(str->chars, str->len);
}

static bool
str_eq(const void* lhs, const void* rhs, size_t size) {
	(void)size;
	const str_t* a = lhs;
	const str_t* b = rhs;
	return a->len == b->len && memcmp(a->chars, b->chars, a->len) == 0;
}

#define NUM_KEYS 100

// Randomized test against a reference array
BTEST(hamt, randomized_against_reference) {
	table_t tbl = { 0 };  // Zero-initialization is valid
	BTEST_ASSERT(bhamt_is_empty(&tbl));

	bool memberships[NUM_KEYS] = { 0 };
	int values[NUM_KEYS] = { 0 };
	int* value_ptrs[NUM_KEYS] = { 0 };

	for (int i = 0; i < 99999; ++i) {
		int key = rand() % NUM_KEYS;
		int value = rand();

		bool existed = memberships[key];
		BTEST_ASSERT_EQUAL("%d", bhamt_has(&tbl, key), existed);

		if (rand() % 2 == 0) {
			bhamt_put(&tbl, key, value, &arena);
		} else {
			bool is_new;
			int* value_ptr = bhamt_upsert_ex(&tbl, key, &arena, &is_new);
			BTEST_ASSERT_EQUAL("%d", is_new, !existed);
			if (is_new) {
				BTEST_ASSERT_EQUAL("%d", *value_ptr, 0);  // New values are zeroed
			} else {
				BTEST_ASSERT_EQUAL("%d", *value_ptr, values[key]);
			}
			*value_ptr = value;
		}
		memberships[key] = true;
		values[key] = value;

		// Nodes never move so value pointers are stable
		int* value_ptr = bhamt_get(&tbl, key);
		BTEST_ASSERT(value_ptr != NULL);
		if (value_ptrs[key] != NULL) {
			BTEST_ASSERT(value_ptrs[key] == value_ptr);
		}
		value_ptrs[key] = value_ptr;

		bhamt_validate(&tbl);
	}

	// Check all keys through both lookup and iteration
	int num_members = 0;
	for (int key = 0; key < NUM_KEYS; ++key) {
		int* value_ptr = bhamt_get(&tbl, key);
		if (memberships[key]) {
			num_members += 1;
			BTEST_ASSERT(value_ptr != NULL);
			BTEST_EXPECT_EQUAL("%d", *value_ptr, values[key]);
		} else {
			BTEST_EXPECT(value_ptr == NULL);
		}
	}

	int num_visited = 0;
	BHAMT_FOREACH(node, &tbl) {
		BTEST_ASSERT(0 <= node->key && node->key < NUM_KEYS);
		BTEST_EXPECT(memberships[node->key]);
		BTEST_EXPECT_EQUAL("%d", node->value, values[node->key]);
		num_visited += 1;
	}
	BTEST_EXPECT_EQUAL("%d", num_visited, num_members);
}

// Forgetting the root empties the table; the arena still owns the memory
BTEST(hamt, forgetting_root_empties_table) {
	table_t tbl = { 0 };
	int key_a = 1;
	int key_b = 3;
	bhamt_put(&tbl, key_a, 2, &arena);
	bhamt_put(&tbl, key_b, 4, &arena);
	BTEST_ASSERT(!bhamt_is_empty(&tbl));

	tbl.root = NULL;
	BTEST_EXPECT(bhamt_is_empty(&tbl));
	BTEST_EXPECT(!bhamt_has(&tbl, key_a));
	BTEST_EXPECT(!bhamt_has(&tbl, key_b));
}

BTEST(hamt, set) {
	set_t set = { 0 };

	int key = 42;
	BTEST_EXPECT(!bhamt_has(&set, key));
	BTEST_EXPECT(bhamt_set_add(&set, key, &arena));
	BTEST_EXPECT(!bhamt_set_add(&set, key, &arena));
	BTEST_EXPECT(bhamt_has(&set, key));
	bhamt_validate(&set);

	int num_visited = 0;
	BHAMT_FOREACH(node, &set) {
		BTEST_EXPECT_EQUAL("%d", node->key, 42);
		num_visited += 1;
	}
	BTEST_EXPECT_EQUAL("%d", num_visited, 1);
}

BTEST(hamt, custom_hash_and_eq) {
	str_table_t tbl;
	bhamt_init(&tbl, str_hash, str_eq);

	static const char* words[] = { "if", "else", "while", "for", "return" };
	int num_words = (int)(sizeof(words) / sizeof(words[0]));
	for (int i = 0; i < num_words; ++i) {
		str_t word = { .chars = words[i], .len = strlen(words[i]) };
		bhamt_put(&tbl, word, i, &arena);
	}

	for (int i = 0; i < num_words; ++i) {
		// A key equal in content but different in identity must match
		char buf[16];
		strcpy(buf, words[i]);
		str_t word = { .chars = buf, .len = strlen(buf) };
		int* value_ptr = bhamt_get(&tbl, word);
		BTEST_ASSERT(value_ptr != NULL);
		BTEST_EXPECT_EQUAL("%d", *value_ptr, i);
	}

	str_t missing = { .chars = "missing", .len = 7 };
	BTEST_EXPECT(bhamt_get(&tbl, missing) == NULL);
	bhamt_validate(&tbl);
}

#define BLIB_IMPLEMENTATION
#include "../../barena.h"
// Route all bhamt allocations through the arena passed as memctx
#define BHAMT_ALLOC(size, align, ctx) barena_memalign((barena_t*)(ctx), (size), (align))
#include "../../bhamt.h"
