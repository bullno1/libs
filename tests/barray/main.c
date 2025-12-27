#include "../../barray.h"
#include "../../btest.h"

static btest_suite_t array = {
	.name = "array",
};

BTEST(array, order) {
	barray(char) arr = NULL;
	barray_push(arr, 3, NULL);
	barray_push(arr, 4, NULL);

	BTEST_EXPECT_EQUAL("%d", arr[0], 3);
	BTEST_EXPECT_EQUAL("%d", arr[1], 4);

	barray_free(arr, NULL);
}

BTEST(array, resize_must_zero) {
	barray(char) arr = NULL;
	barray_push(arr, 3, NULL);
	barray_resize(arr, 4, NULL);

	BTEST_EXPECT_EQUAL("%d", arr[0], 3);
	for (int i = 1; i < 4; ++i) {
		BTEST_EXPECT_EQUAL("%d", arr[i], 0);
	}

	barray_free(arr, NULL);
}

#define BTEST_INCLUDE_DEFAULT_RUNNER
#define BLIB_IMPLEMENTATION
#include "../../barray.h"
#include "../../btest.h"
#include "../../blog.h"
