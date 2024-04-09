#include "../../autolist.h"
#include <stdio.h>

AUTOLIST_DECLARE(number_list)

int main(int argc, const char* argv[]) {
	for (
		const autolist_entry_t* const* itr = AUTOLIST_BEGIN(number_list);
		itr != AUTOLIST_END(number_list);
		++itr
	) {
		// Skip padding
		if (*itr == NULL) { continue; }

		const autolist_entry_t* entry = *itr;
		printf("%.*s = %d\n", (int)entry->name_length, entry->name, *(int*)entry->value_addr);
	}

	return 0;
}
