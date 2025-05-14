#include "../../autolist.h"
#include <stdio.h>

AUTOLIST_DECLARE(number_list)

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	AUTOLIST_FOREACH(itr, number_list) {
		printf("%.*s = %d\n", (int)itr->name_length, itr->name, *(int*)itr->value_addr);
	}

	return 0;
}

AUTOLIST_IMPL(number_list)
