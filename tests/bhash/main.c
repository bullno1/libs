#define BLIB_IMPLEMENTATION
#include "../../bhash.h"
#include <stdlib.h>
#include <assert.h>

typedef BHASH_TABLE(int, char) table_t;

enum {
	BHASH_TEST_ADD,
	BHASH_TEST_REMOVE,
	BHASH_TEST_POP,
	BHASH_TEST_COUNT,
};

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	table_t tbl;
	bhash_init(&tbl, bhash_config_default());

	// A simple boolean table to track membership of each number in [0, 10)
	bool memberships[10] = { 0 };

	for (int i = 0; i < 99999; ++i) {
		/*printf("i = %d, len = %d\n", i, bhash_len(&tbl));*/

		int action = rand() % BHASH_TEST_COUNT;
		int key = rand() % 10;

		bhash_index_t index;
		(void)index;

		if (action == BHASH_TEST_ADD) {
			index = bhash_find(&tbl, key);
			bhash_index_t len_before = bhash_len(&tbl);
			bool existed = bhash_is_valid(index);

			/*printf("Add %d -> %d\n", key, key * 2);*/
			bhash_put(&tbl, key, (char){ (char)key * 2 });
			memberships[key] = true;

			bhash_index_t len_after = bhash_len(&tbl);
			if (existed) {
				BHASH_ASSERT(len_after == len_before, "%s: %d -> %d", len_before, len_after);
			} else {
				BHASH_ASSERT(len_after == len_before + 1, "%s: %d -> %d", len_before, len_after);
			}

			index = bhash_find(&tbl, key);

			assert(bhash_is_valid(index));
			BHASH_ASSERT(tbl.keys[index] == key, "%s: %d vs %d", tbl.keys[index], key);
			BHASH_ASSERT(tbl.values[index] == key * 2, "%s: %d vs %d", tbl.values[index], key * 2);
		} else if (action == BHASH_TEST_REMOVE) {
			/*printf("Remove %d\n", key);*/
			bhash_index_t len_before = bhash_len(&tbl);
			index = bhash_remove(&tbl, key);
			bhash_index_t len_after = bhash_len(&tbl);
			memberships[key] = false;

			if (bhash_is_valid(index)) {
				BHASH_ASSERT(len_after == len_before - 1, "%s: %d -> %d", len_before, len_after);
				BHASH_ASSERT(tbl.keys[index] == key, "%s: %d -> %d", tbl.keys[index], key);
				BHASH_ASSERT(tbl.values[index] == key * 2, "%s: %d -> %d", tbl.values[index], key * 2);
			} else {
				BHASH_ASSERT(len_after == len_before, "%s: %d -> %d", len_before, len_after);
			}

			index = bhash_find(&tbl, key);
			assert(!bhash_is_valid(index));
		} else if (action == BHASH_TEST_POP && bhash_len(&tbl) > 0) {
			bhash_index_t len_before = bhash_len(&tbl);
			int key_to_remove = tbl.keys[0];
			/*printf("Remove %d\n", key_to_remove);*/
			index = bhash_remove(&tbl, key_to_remove);
			memberships[key_to_remove] = false;
			bhash_index_t len_after = bhash_len(&tbl);

			assert(bhash_is_valid(index));
			BHASH_ASSERT(len_after == len_before - 1, "%s: %d -> %d", len_before, len_after);
			BHASH_ASSERT(tbl.keys[index] == key_to_remove, "%s: %d -> %d", tbl.keys[index], key_to_remove);
			BHASH_ASSERT(tbl.values[index] == key_to_remove * 2, "%s: %d -> %d", tbl.values[index], key_to_remove * 2);

			index = bhash_find(&tbl, key_to_remove);
			assert(!bhash_is_valid(index));
		}

		bhash_validate(&tbl);

		int size = 0;
		for (int j = 0; j < 10; ++j) {
			index = bhash_find(&tbl, j);
			BHASH_ASSERT(
				bhash_is_valid(index) == memberships[j],
				"%s: Membership mismatch for %d",
				j
			);

			if (memberships[j]) { size += 1; }
			if (bhash_is_valid(index) > 0) {
				BHASH_ASSERT(tbl.keys[index] == j, "%s: Key mismatch: %d vs %d", tbl.keys[index], j);
				BHASH_ASSERT(tbl.values[index] == j * 2, "%s: Value mismatch: %d vs %d", tbl.values[index], j * 2);
			}
		}

		BHASH_ASSERT(size == bhash_len(&tbl), "%s: Size mismatch: %d vs %d", size, bhash_len(&tbl));
	}

	bhash_cleanup(&tbl);

	return 0;
}
