#include "../../autolist.h"
#include <stdio.h>

AUTOLIST_DECLARE(number_list)

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	AUTOLIST_FOREACH(itr, number_list) {
		const autolist_entry_t* entry = *itr;
		printf("%.*s = %d\n", (int)entry->name_length, entry->name, *(int*)entry->value_addr);
	}

	return 0;
}

AUTOLIST_IMPL(number_list)
