#include "../../reglist.h"
#include <stdio.h>

REGLIST_DECLARE(number_list)

int main(int argc, const char* argv[]) {
	for (
		const reglist_entry_t* const* itr = REGLIST_BEGIN(number_list);
		itr != REGLIST_END(number_list);
		++itr
	) {
		// Skip padding
		if (*itr == NULL) { continue; }

		const reglist_entry_t* entry = *itr;
		printf("%.*s = %d\n", (int)entry->name_length, entry->name, *(int*)entry->value_addr);
	}

	return 0;
}
