#include "common.h"
#include <stdio.h>
#define BSERIAL_IMPLEMENTATION
#include "../../bserial.h"

AUTOLIST_DECLARE(bserial_test)

int main(int argc, const char* argv[]) {
	AUTOLIST_FOREACH(itr, bserial_test) {
		const autolist_entry_t* entry = *itr;

		const test_t* test = entry->value_addr;
		printf("--- %s/%s ---\n", test->suite->name, test->name);
		void* fixture = NULL;
		if (test->suite->init != NULL) {
			test->suite->init();
		}
		test->run(fixture);
		if (test->suite->cleanup != NULL) {
			test->suite->cleanup();
		}
	}
}
