#include "../../bhash.h"
#include <stdlib.h>
#include <assert.h>

typedef BHASH_TABLE(int, int) table_t;

enum {
	BHASH_TEST_ADD,
	BHASH_TEST_REMOVE,
	BHASH_TEST_POP,
	BHASH_TEST_COUNT,
};

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	bhash_config_t config = {
		.eq = bhash_eq,
		.hash = bhash_hash,
		.has_values = true,
		.removable = true,
	};
	table_t* tbl = NULL;

	// A simple boolean table to track membership of each number in [0, 10)
	bool memberships[10] = { 0 };

	for (int i = 0; i < 99999; ++i) {
		printf("i = %d, len = %d\n", i, bhash_len(tbl));

		int action = rand() % BHASH_TEST_COUNT;
		int key = rand() % 10;

		bhash_index_t index;
		(void)index;

		if (action == BHASH_TEST_ADD) {
			bhash_find(index, tbl, key, config);
			bhash_index_t len_before = bhash_len(tbl);
			bool existed = bhash_is_valid(index);

			printf("Add %d -> %d\n", key, key * 2);
			bhash_put(tbl, key, key * 2, config);
			memberships[key] = true;

			bhash_index_t len_after = bhash_len(tbl);
			if (existed) {
				BHASH_ASSERT(len_after == len_before, "%s: %d -> %d", len_before, len_after);
			} else {
				BHASH_ASSERT(len_after == len_before + 1, "%s: %d -> %d", len_before, len_after);
			}

			bhash_find(index, tbl, key, config);

			assert(bhash_is_valid(index));
			BHASH_ASSERT(bhash_keys(tbl)[index] == key, "%s: %d vs %d", bhash_keys(tbl)[index], key);
			BHASH_ASSERT(bhash_values(tbl)[index] == key * 2, "%s: %d vs %d", bhash_values(tbl)[index], key * 2);
		} else if (action == BHASH_TEST_REMOVE) {
			printf("Remove %d\n", key);
			bhash_index_t len_before = bhash_len(tbl);
			bhash_remove(index, tbl, key, config);
			bhash_index_t len_after = bhash_len(tbl);
			memberships[key] = false;

			if (bhash_is_valid(index)) {
				BHASH_ASSERT(len_after == len_before - 1, "%s: %d -> %d", len_before, len_after);
				BHASH_ASSERT(bhash_keys(tbl)[index] == key, "%s: %d -> %d", bhash_keys(tbl)[index], key);
				BHASH_ASSERT(bhash_values(tbl)[index] == key * 2, "%s: %d -> %d", bhash_values(tbl)[index], key * 2);
			} else {
				BHASH_ASSERT(len_after == len_before, "%s: %d -> %d", len_before, len_after);
			}

			bhash_find(index, tbl, key, config);
			assert(!bhash_is_valid(index));
		} else if (action == BHASH_TEST_POP && bhash_len(tbl) > 0) {
			bhash_index_t len_before = bhash_len(tbl);
			int key = bhash_keys(tbl)[0];
			printf("Remove %d\n", key);
			bhash_remove(index, tbl, key, config);
			memberships[key] = false;
			bhash_index_t len_after = bhash_len(tbl);

			assert(bhash_is_valid(index));
			BHASH_ASSERT(len_after == len_before - 1, "%s: %d -> %d", len_before, len_after);
			BHASH_ASSERT(bhash_keys(tbl)[index] == key, "%s: %d -> %d", bhash_keys(tbl)[index], key);
			BHASH_ASSERT(bhash_values(tbl)[index] == key * 2, "%s: %d -> %d", bhash_values(tbl)[index], key * 2);

			bhash_find(index, tbl, key, config);
			assert(!bhash_is_valid(index));
		}

		bhash_validate(tbl, config);

		int size = 0;
		for (int i = 0; i < 10; ++i) {
			bhash_index_t index;
			bhash_find(index, tbl, i, config);
			BHASH_ASSERT(
				bhash_is_valid(index) == memberships[i],
				"%s: Membership mismatch for %d",
				i
			);

			if (memberships[i]) { size += 1; }
		}

		BHASH_ASSERT(size == bhash_len(tbl), "%s: Size mismatch: %d vs %d", size, bhash_len(tbl));
	}

	bhash_destroy(tbl, config);

	return 0;
}
