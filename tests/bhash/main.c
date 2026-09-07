#include "../../bhash.h"
#include "../../btest.h"
#include <stdlib.h>

typedef BHASH_TABLE(int, char) table_t;

enum {
	BHASH_TEST_ADD,
	BHASH_TEST_REMOVE,
	BHASH_TEST_POP,
	BHASH_TEST_COUNT,
};

static btest_suite_t bhash_ = {
	.name = "bhash",
};

BTEST(bhash_, randomized_against_reference) {
	table_t tbl;
	bhash_init(&tbl, NULL);

	// A simple boolean table to track membership of each number in [0, 10)
	bool memberships[10] = { 0 };

	for (int i = 0; i < 99999; ++i) {
		int action = rand() % BHASH_TEST_COUNT;
		int key = rand() % 10;

		bhash_index_t index;
		(void)index;

		if (action == BHASH_TEST_ADD) {
			index = bhash_find(&tbl, key);
			bhash_index_t len_before = bhash_len(&tbl);
			bool existed = bhash_is_valid(index);

			bhash_put(&tbl, key, (char){ (char)key * 2 });
			memberships[key] = true;

			bhash_index_t len_after = bhash_len(&tbl);
			if (existed) {
				BTEST_ASSERT_EX(len_after == len_before, "%d -> %d", len_before, len_after);
			} else {
				BTEST_ASSERT_EX(len_after == len_before + 1, "%d -> %d", len_before, len_after);
			}

			index = bhash_find(&tbl, key);

			BTEST_ASSERT(bhash_is_valid(index));
			BTEST_ASSERT_EX(tbl.keys[index] == key, "%d vs %d", tbl.keys[index], key);
			BTEST_ASSERT_EX(tbl.values[index] == key * 2, "%d vs %d", tbl.values[index], key * 2);
		} else if (action == BHASH_TEST_REMOVE) {
			bhash_index_t len_before = bhash_len(&tbl);
			index = bhash_remove(&tbl, key);
			bhash_index_t len_after = bhash_len(&tbl);
			memberships[key] = false;

			if (bhash_is_valid(index)) {
				BTEST_ASSERT_EX(len_after == len_before - 1, "%d -> %d", len_before, len_after);
				BTEST_ASSERT_EX(tbl.keys[index] == key, "%d -> %d", tbl.keys[index], key);
				BTEST_ASSERT_EX(tbl.values[index] == key * 2, "%d -> %d", tbl.values[index], key * 2);
			} else {
				BTEST_ASSERT_EX(len_after == len_before, "%d -> %d", len_before, len_after);
			}

			index = bhash_find(&tbl, key);
			BTEST_ASSERT(!bhash_is_valid(index));
		} else if (action == BHASH_TEST_POP && bhash_len(&tbl) > 0) {
			bhash_index_t len_before = bhash_len(&tbl);
			int key_to_remove = tbl.keys[0];
			index = bhash_remove(&tbl, key_to_remove);
			memberships[key_to_remove] = false;
			bhash_index_t len_after = bhash_len(&tbl);

			BTEST_ASSERT(bhash_is_valid(index));
			BTEST_ASSERT_EX(len_after == len_before - 1, "%d -> %d", len_before, len_after);
			BTEST_ASSERT_EX(tbl.keys[index] == key_to_remove, "%d -> %d", tbl.keys[index], key_to_remove);
			BTEST_ASSERT_EX(tbl.values[index] == key_to_remove * 2, "%d -> %d", tbl.values[index], key_to_remove * 2);

			index = bhash_find(&tbl, key_to_remove);
			BTEST_ASSERT(!bhash_is_valid(index));
		}

		bhash_validate(&tbl);

		int size = 0;
		for (int j = 0; j < 10; ++j) {
			index = bhash_find(&tbl, j);
			BTEST_ASSERT_EX(
				bhash_is_valid(index) == memberships[j],
				"Membership mismatch for %d",
				j
			);

			if (memberships[j]) { size += 1; }
			if (bhash_is_valid(index) > 0) {
				BTEST_ASSERT_EX(tbl.keys[index] == j, "Key mismatch: %d vs %d", tbl.keys[index], j);
				BTEST_ASSERT_EX(tbl.values[index] == j * 2, "Value mismatch: %d vs %d", tbl.values[index], j * 2);
			}
		}

		BTEST_ASSERT_EX(size == bhash_len(&tbl), "Size mismatch: %d vs %d", size, bhash_len(&tbl));
	}

	bhash_cleanup(&tbl);
}

#define BLIB_IMPLEMENTATION
#include "../../bhash.h"
