#include "common.h"
#include <stdio.h>
#define BLIB_IMPLEMENTATION
#include "../../bserial.h"
#include "../../barena.h"

AUTOLIST_DECLARE(bserial_test)

int main(int argc, const char* argv[]) {
	AUTOLIST_FOREACH(itr, bserial_test) {
		const test_t* test = itr->value_addr;
		printf("--- %s/%s ---\n", test->suite->name, test->name);
		if (test->suite->init != NULL) {
			test->suite->init();
		}
		test->run();
		if (test->suite->cleanup != NULL) {
			test->suite->cleanup();
		}
	}
}

AUTOLIST_IMPL(bserial_test)
