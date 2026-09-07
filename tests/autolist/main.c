#include "../../autolist.h"
#include "../../btest.h"
#include <string.h>

// Entries are registered from a.c and b.c, this unit only owns the list
AUTOLIST_DECLARE(number_list)

static btest_suite_t autolist_ = {
	.name = "autolist",
};

BTEST(autolist_, collects_entries_from_all_units) {
	int num_entries = 0;
	bool found_a = false;
	bool found_b = false;

	AUTOLIST_FOREACH(itr, number_list) {
		++num_entries;
		BTEST_EXPECT_EQUAL("%zu", itr->value_size, sizeof(int));
		int value = *(int*)itr->value_addr;

		if (itr->name_length == 1 && strncmp(itr->name, "a", 1) == 0) {
			found_a = true;
			BTEST_EXPECT_EQUAL("%d", value, 1);
		} else if (itr->name_length == 1 && strncmp(itr->name, "b", 1) == 0) {
			found_b = true;
			BTEST_EXPECT_EQUAL("%d", value, 2);
		} else {
			BTEST_EXPECT_EX(false, "unexpected entry %.*s", (int)itr->name_length, itr->name);
		}
	}

	BTEST_EXPECT_EQUAL("%d", num_entries, 2);
	BTEST_EXPECT(found_a);
	BTEST_EXPECT(found_b);
}

AUTOLIST_IMPL(number_list)
