#include <assert.h>
#include <stdbool.h>

// Lets a test look at what happens past an assertion, as in a build without them
bool bent_test_tolerate_asserts = false;
int bent_test_num_failed_asserts = 0;

#define BENT_ASSERT(COND) \
	do { \
		if (!(COND)) { \
			if (bent_test_tolerate_asserts) { \
				++bent_test_num_failed_asserts; \
			} else { \
				assert(!#COND); \
			} \
		} \
	} while (0)

#include "shared.h"

BENT_DEFINE_COMP(basic_component) = {
	.size = sizeof(int),
	.flags = BENT_COMP_TRANSIENT,
};

BENT_DEFINE_COMP(basic_component2) = {
	.size = sizeof(float),
	.flags = BENT_COMP_TRANSIENT,
};

#define BLIB_IMPLEMENTATION
#include "../../bent.h"
